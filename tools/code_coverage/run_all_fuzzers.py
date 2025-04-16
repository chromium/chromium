# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Run all Chromium libfuzzer targets that have corresponding corpora,
   then save the profdata files.

  * Example usage: run_all_fuzzers.py --fuzzer-binaries-dir foo
                   --fuzzer-corpora-dir bar --profdata-outdir baz
"""

import argparse
import abc
import dataclasses
import glob
import json
import logging
import math
import os
import pathlib
import subprocess
import sys
import shutil
import tempfile

from multiprocessing import Process, Manager, cpu_count, Pool
from typing import Mapping, Sequence, Optional

WHOLE_CORPUS_TIMEOUT_SECS = 1200
INDIVIDUAL_TESTCASE_TIMEOUT_SECS = 60
INDIVIDUAL_TESTCASES_MAX_TO_TRY = 500
INDIVIDUAL_TESTCASES_SUCCESSES_NEEDED = 100
MAX_FILES_PER_CHUNK = 80
CHUNK_EXECUTION_TIMEOUT = 400
MIN_FILES_FOR_CHUNK_STRATEGY = 30
MIN_CHUNK_NUMBER = 10

LIBFUZZER = 'libfuzzer'
CENTIPEDE = 'centipede'
FUZZILLI = 'fuzzilli'
ALL_FUZZER_TYPES = [LIBFUZZER, CENTIPEDE, FUZZILLI]
REPORT_DIR = 'out/report'

LLVM_PROFDATA = 'third_party/llvm-build/Release+Asserts/bin/llvm-profdata'


class EngineRunner(abc.ABC):
  """This class abstracts running different engines against a full corpus or a
  bunch of testcases. Implementers might provide different running commands
  depending on the parameters.
  """

  @abc.abstractmethod
  def run_full_corpus(self, env: Mapping[str, str], timeout: float,
                      annotation: str, corpus_dir: Optional[str]) -> bool:
    """Runs the current engine against the full corpus. It returns True if the
    command succeeded and False otherwise.

    Args:
        env: the extra environment to forward to the command.
        timeout: the potential timeout for the command.
        annotation: some annotations for the command.
        corpus_dir: optional corpus directory to run the engine against. If
        None, this will run the target without any testcase (does nothing).

    Returns:
        whether the run succeed.
    """
    pass

  @abc.abstractmethod
  def run_testcases(self, env: Mapping[str, str], timeout: float,
                    annotation: str, testcases: Sequence[str]) -> bool:
    """Runs the current engine against some testcases (can be one). It returns
    True if the command succeeded and False otherwise.

    Args:
        env: the extra environment to forward to the command.
        timeout: the potential timeout for the command.
        annotation: some annotations for the command.
        testcases: the sequence of testcases.

    Returns:
        whether the run succeed.
    """
    pass

  def _run_command(self, cmd: Sequence[str], env: Mapping[str, str],
                   timeout: float, annotation: str) -> bool:
    return _run_and_log(cmd, env, timeout, annotation)


@dataclasses.dataclass
class CmdRunner(EngineRunner):
  """A simple command runner. Depending on whether it's running in full corpus
  mode or testcases mode, this will simply append the extra parameters at the
  end of the provided command.
  """
  cmd: Sequence[str]

  def run_full_corpus(self, env: Mapping[str, str], timeout: float,
                      annotation: str, corpus_dir: Optional[str]) -> bool:
    extra_args = []
    if corpus_dir:
      extra_args += [corpus_dir]
    return self._run_command(self.cmd + extra_args, env, timeout, annotation)

  def run_testcases(self, env: Mapping[str, str], timeout: float,
                    annotation: str, testcases: Sequence[str]) -> bool:
    return self._run_command(self.cmd + testcases, env, timeout, annotation)


@dataclasses.dataclass
class CentipedeRunner(EngineRunner):
  """Runs a given target with the centipede fuzzing engine.
  """
  centipede_path: str
  fuzz_target_path: str

  def run_full_corpus(self, env: Mapping[str, str], timeout: float,
                      annotation: str, corpus_dir: Optional[str]) -> bool:
    workdir = tempfile.TemporaryDirectory()
    tmpdir = tempfile.TemporaryDirectory()
    this_env = env.copy()
    this_env['TMPDIR'] = tmpdir.name
    cmd = [
        self.centipede_path, f'-binary={self.fuzz_target_path}',
        '-shmem_size_mb=4096', '-address_space_limit_mb=0', '-rss_limit_mb=0',
        '-symbolizer_path=/dev/null', '-num_runs=0', '-require_pc_table=false',
        f'-workdir={workdir.name}', '-populate_binary_info=false',
        '-batch_triage_suspect_only', '-ignore_timeout_reports=true',
        '-exit_on_crash=true'
    ]
    if corpus_dir:
      cmd += [f'-corpus_dir={corpus_dir}']
    return self._run_command(cmd, this_env, timeout, annotation)

  def run_testcases(self, env: Mapping[str, str], timeout: float,
                    annotation: str, testcases: Sequence[str]) -> bool:
    res = self._run_command([self.fuzz_target_path] + testcases, env, timeout,
                            annotation)
    # running Centipede in that particular mode will generate feature files for
    # each of the testcase. Since we're running in an environment with limited
    # disk space, we must delete those files after the run.
    for testcase in testcases:
      feature_file = f'{testcase}-features'
      if os.path.exists(feature_file):
        os.unlink(feature_file)
    return res


@dataclasses.dataclass
class FuzzilliRunner(EngineRunner):
  """Runs a given target with Fuzzilli.
  """
  d8_path: str
  target_arguments: Sequence[str]
  source_dir: str

  def __post_init__(self):
    self.d8_path = os.path.abspath(self.d8_path)
    self.source_dir = os.path.abspath(self.source_dir)

  @property
  def cmd(self):
    return [self.d8_path] + self.target_arguments

  def run_full_corpus(self, env: Mapping[str, str], timeout: float,
                      annotation: str, corpus_dir: Optional[str]) -> bool:
    # This is not supported for the `d8` runner, so we simply return False so
    # that user can only use `run_testcases` inherited from CmdRunner.
    return False

  def run_testcases(self, env: Mapping[str, str], timeout: float,
                    annotation: str, testcases: Sequence[str]) -> bool:
    run_dir = tempfile.TemporaryDirectory()
    os.symlink(os.path.join(self.source_dir, 'v8/test/'),
               os.path.join(run_dir.name, 'test'))
    # We need to run the test cases separately, because otherwise the JS files
    # will be ran in the same JS namespace.
    for testcase in testcases:
      testcase = os.path.abspath(testcase)
      _run_and_log(cmd=self.cmd + [testcase],
                   env=env,
                   timeout=timeout,
                   annotation=annotation,
                   cwd=run_dir.name)
    return True


class ChromeRunner(CmdRunner):
  """Runs chrome. This needs special handling because the run will always fail,
  but we still want to consider the run successful.
  """

  def run_full_corpus(self, env, timeout, annotation, corpus_dir):
    super().run_full_corpus(env, timeout, annotation, corpus_dir)
    return True


def _profdata_merge(inputs: Sequence[str], output: str) -> bool:
  """Merges the given profraw files into a single file.

  Deletes any inputs, whether or not it succeeded.

  Args:
    inputs: paths to input files.
    output: output file path.

  Returns:
    True if it worked.
  """
  llvm_profdata_cmd = [LLVM_PROFDATA, 'merge', '-sparse'
                       ] + inputs + ['-o', output]
  try:
    subprocess.check_call(llvm_profdata_cmd)
    return True
  except Exception as e:
    # TODO(crbug.com/328849489: investigate failures
    logging.warning("profdata merge failed, treating this target as failed")
  finally:
    for f in inputs:
      if os.path.exists(f):
        os.unlink(f)
  return False


def _run_and_log(cmd: Sequence[str],
                 env: Mapping[str, str],
                 timeout: float,
                 annotation: str,
                 cwd: str = None) -> bool:
  """Runs a given command and logs output in case of failure.

  Args:
    cmd: the command and its arguments.
    env: environment variables to apply.
    timeout: the timeout to apply, in seconds.
    annotation: annotation to add to logging.

  Returns:
    True iff the command ran successfully.
  """
  logging.debug('Trying command: %s (%s)', cmd, annotation)
  try:
    subprocess.run(cmd,
                   env=env,
                   timeout=timeout,
                   capture_output=True,
                   check=True,
                   cwd=cwd)
    return True
  except Exception as e:
    if type(e) == subprocess.TimeoutExpired:
      logging.warning('Command %s (%s) timed out after %s seconds', cmd,
                      annotation, e.timeout)
    else:
      logging.warning(
          'Command %s (%s) return code: %i\nStdout:\n%s\nStderr:\n%s', cmd,
          annotation, e.returncode, e.output, e.stderr)
  return False


def _erase_profraws(pattern):
  """Erases any pre-existing profraws matching a LLVM_PROFILE_FILE pattern.

  Parameters:
    pattern: An LLVM_PROFILE_FILE environment variable value, which may
      contain %p for a process ID
  """
  pattern = pattern.replace("%p", "*")
  for f in glob.iglob(pattern):
    os.unlink(f)


def _matching_profraws(pattern):
  """Returns a list of filenames matching a given LLVM_PROFILE_FILE pattern.

  Parameters:
    pattern: An LLVM_PROFILE_FILE environment variable value, which may
      contain %p for a process ID
  """
  pattern = pattern.replace("%p", "*")
  return [f for f in glob.iglob(pattern) if os.path.getsize(f) > 0]


def _accumulated_profdata_merge(inputs: Sequence[str], profdata: str) -> bool:
  """Accumulate profdata from inputs and potentially existing profdata file
  into profdata file itself. `inputs` file will be deleted independently of the
  function result. If this function fails and `profdata` file exists, its
  contents will be preserved.

  Args:
      inputs: a sequence of input files.
      profdata: the resulting profdata file (may or may not exist).

  Returns:
      whether the merge succeeded/
  """
  # If the profdata file doesn't exist yet, we can just run the normal merging
  # function.
  if not os.path.exists(profdata):
    return _profdata_merge(inputs, profdata)

  # This file will be used as a clone of the initial profdata file.
  copy = tempfile.NamedTemporaryFile()
  # This file will be used as a copy of the profdata file to be used as input
  # of the _profdata_merge function. It will always be deleted by
  # `_profdata_merge`, so we disable `delete` to avoid a warning from cpython.
  file = tempfile.NamedTemporaryFile(delete=False)
  shutil.copy2(profdata, copy.name)
  shutil.copy2(profdata, file.name)
  res = _profdata_merge(inputs + [file.name], profdata)
  if not res:
    # If the merge wasn't successful, let's ensure that the profdata file is
    # reverted with its previous content. This helps keep track of the profile
    # information gathered from the successful runs.
    shutil.copy2(copy.name, profdata)
  return res


def _get_target_corpus_files(target_details) -> Sequence[str]:
  """Lists the corpus files for the given target. This correctly handles the
  different target setup such as the ones providing neither corpus files nor
  corpus directory.

  Args:
      target_details: the target details.

  Returns:
      the list of corpus files associated with the target.
  """
  corpus_dir = target_details['corpus']
  corpus_files = target_details['files']
  if not corpus_dir and (not corpus_files or corpus_files == '*'):
    return []

  if corpus_files and corpus_files != '*':
    return corpus_files

  corpus_files = os.listdir(corpus_dir)
  corpus_files = [os.path.join(corpus_dir, e) for e in corpus_files]
  return corpus_files


def _split_corpus_files_into_chunks(corpus_files: Sequence[str]):
  assert len(corpus_files) >= MIN_FILES_FOR_CHUNK_STRATEGY
  if len(corpus_files) < MAX_FILES_PER_CHUNK * (MIN_CHUNK_NUMBER - 1):
    chunk_num = int(len(corpus_files) / MIN_CHUNK_NUMBER)
  else:
    chunk_num = MAX_FILES_PER_CHUNK
  chunks = [
      corpus_files[i:i + chunk_num]
      for i in range(0, len(corpus_files), chunk_num)
  ]
  return chunks


def _run_full_corpus(target_details) -> bool:
  """Runs a full corpus strategy.

  Args:
      target_details: the target details.

  Returns:
    whether the strategy succeeded or not.
  """
  target = target_details['name']
  cmd_runner = target_details['cmd_runner']
  env = target_details['env']
  corpus_dir = target_details['corpus']
  target_profdata = target_details['profdata_file']

  logging.info('[%s][full corpus] starting', target)

  profraw_dir = tempfile.TemporaryDirectory()
  fullcorpus_profraw = os.path.join(profraw_dir.name, target + "_%p.profraw")
  env['LLVM_PROFILE_FILE'] = fullcorpus_profraw
  if cmd_runner.run_full_corpus(env, WHOLE_CORPUS_TIMEOUT_SECS,
                                'full corpus attempt', corpus_dir):
    matching_profraws = list(_matching_profraws(fullcorpus_profraw))
    logging.info('[%s][full corpus] merging %s into %s', target,
                 matching_profraws, target_profdata)
    if _profdata_merge(matching_profraws, target_profdata):
      logging.info('[%s][full corpus] done, success', target)
      return True

  logging.info('[%s][full corpus] done, failure', target)
  return False


def _run_corpus_in_chunks(target_details) -> bool:
  """Runs the chunk strategy. This strategy consists of running the target's
  corpora into multiple chunks in case some testcases are preventing the binary
  from making any progress with the remaining files.

  Args:
      target_details: the target details.

  Returns:
      whether the strategy succeeded or not.
  """
  target = target_details['name']
  cmd_runner = target_details['cmd_runner']
  env = target_details['env']
  target_profdata = target_details['profdata_file']

  corpus_files = _get_target_corpus_files(target_details)
  if not corpus_files:
    logging.info('[%s][chunk strategy] cannot get corpus files, aborting',
                 target)
    return False

  if len(corpus_files) < MIN_FILES_FOR_CHUNK_STRATEGY:
    logging.info('[%s][chunk strategy] number of corpus files too low %i',
                 target, len(corpus_files))
    return False

  chunks = _split_corpus_files_into_chunks(corpus_files)
  profdata_dir = tempfile.TemporaryDirectory()
  temp_target_profdata = os.path.join(profdata_dir.name, f'{target}.profdata')
  failed_chunks = []
  logging.info('[%s][chunk strategy] starting, %i chunks to run', target,
               len(chunks))

  # Let's run the fuzzer chunks by chunks. If it fails too much, we early bail
  # out to avoid spending too much time on a target that most likely won't give
  # good results.
  for idx, chunk in enumerate(chunks):
    logging.info('[%s][chunk strategy] running chunk %i / %i', target, idx,
                 len(chunks))
    profraw_dir = tempfile.TemporaryDirectory()
    fullcorpus_profraw = os.path.join(profraw_dir.name,
                                      f'{target}_{idx}_%p.profraw')
    env['LLVM_PROFILE_FILE'] = fullcorpus_profraw
    chunk_profdata = os.path.join(profdata_dir.name, f'{target}_{idx}.profdata')
    if cmd_runner.run_testcases(env, CHUNK_EXECUTION_TIMEOUT,
                                f"Running chunk {idx}", chunk):
      matching_profraws = list(_matching_profraws(fullcorpus_profraw))
      if _profdata_merge(matching_profraws, chunk_profdata):
        # we accumulate the profile data to avoid taking too much disk space.
        if not _accumulated_profdata_merge([chunk_profdata],
                                           temp_target_profdata):
          logging.warning(
              '[%s][chunk strategy] accumulation failed for chunk %i', target,
              idx)
        continue
    failed_chunks.append(chunk)
    failure_rate = len(failed_chunks) / (idx + 1)
    logging.debug(
        '[%s][chunk strategy] chunk failed (%i / %i), failure rate %.2f',
        target, idx, len(chunks), failure_rate)
    if idx > 4 and failure_rate > 0.75:
      # This is mostly to exclude always failing fuzzers and avoid wasting time
      # on that.
      logging.warning(
          '[%s][chunk strategy] chunk failrue rate (%.2f) too high, stopping',
          target, failure_rate)
      return False

  # Sometimes, some state sensitive fuzzers spuriously fail when ran in batch,
  # but almost always succeed when ran test cases by test case. For that
  # reason, we limit the number of test cases to run to the limit of test cases
  # the last strategy will run. This ensures this strategy performs at least
  # similarly (but very often better) than the test case per test case
  # strategy.
  if sum(len(c) for c in failed_chunks) > INDIVIDUAL_TESTCASES_MAX_TO_TRY:
    logging.warning(
        '[%s][chunk strategy] flaky fuzzer,'
        ' will shrink the number of test cases to retry', target)
    failed_testcases = [e for chunk in failed_chunks for e in chunk]
    failed_testcases = failed_testcases[:INDIVIDUAL_TESTCASES_MAX_TO_TRY]
    failed_chunks = _split_corpus_files_into_chunks(failed_testcases)

  # We delay processing the failed chunk because we want to make sure the
  # strategy hasn't failed earlier. Note that we still rely on `_run_testcases`
  # to bail out early if the chunk contains too much test cases that runs into
  # errors.
  for idx, chunk in enumerate(failed_chunks):
    chunk_profdata = os.path.join(profdata_dir.name,
                                  f'{target}_{idx + len(chunks)}.profdata')
    if _run_testcases(target, cmd_runner, env, chunk, chunk_profdata):
      # we accumulate the profile data to avoid taking too much disk space.
      _accumulated_profdata_merge([chunk_profdata], temp_target_profdata)
  if os.path.exists(temp_target_profdata):
    shutil.copy2(temp_target_profdata, target_profdata)
  logging.info('[%s][chunk strategy] done, success', target)
  return os.path.exists(target_profdata)


def _run_testcases(target: str, runner: EngineRunner, env: Mapping[str, str],
                   testcases: Sequence[str], target_profdata: str) -> bool:
  """Runs the given testcases and tries to generate a profdata file out of the
  runs. If the testcases are failing too frequently, the execution will be
  stopped, but the profile file might still be generated.

  Args:
      target: the target name.
      runner: the engine runner.
      env: the environment.
      testcases: the list of test cases to run.
      target_profdata: the profdata to write to.

  Returns:
      whether it succeeded or not.
  """
  profraw_dir = tempfile.TemporaryDirectory()
  profraw_file = os.path.join(profraw_dir.name, f'testcase_strategy_%p.profraw')
  env['LLVM_PROFILE_FILE'] = profraw_file
  failures = 0
  total_runs = 0
  logging.info('[%s][testcase strategy] starting, %i inputs to run', target,
               len(testcases))
  for testcase in testcases:
    if total_runs > 5 and failures / total_runs > 0.75:
      logging.warning(
          '[%s][testcase strategy] abandonning, too much failures...', target)
      break
    if not runner.run_testcases(env=env,
                                timeout=INDIVIDUAL_TESTCASE_TIMEOUT_SECS,
                                annotation="testcase runner",
                                testcases=[testcase]):
      failures += 1
    total_runs += 1
    matching_profraws = list(_matching_profraws(profraw_file))
    _accumulated_profdata_merge(matching_profraws, target_profdata)
  res = os.path.exists(target_profdata)
  res_str = 'success' if res else 'failure'
  logging.info('[%s][testcase strategy] done, %s', target, res_str)
  return res


def _run_fuzzer_target(args):
  """Runs a given fuzzer target. Designed to be called in parallel.

  Parameters:
    args[0]: A dict containing information about what to run. Must contain:
      name: name of the fuzzer target
      corpus_dir: where to find its corpus. May be None.
      profraw_dir: the directory in which to create a .profraws temporarily
      profdata_file: the output .profdata filename to create
      env: a dict of additional environment variables. This function will
        append profdata environment variables.
      cmd: a list of command line arguments, including the binary name.
        This function will append corpus entries.
    args[1]: A multiprocessing.Manager.list for names of successful fuzzers.
    args[2]: A multiprocessing.Manager.list for names of failed fuzzers.
    args[3]: The number of targets (for logging purposes only)

  Returns:
    None.
  """
  target_details = args[0]
  verified_fuzzer_targets = args[1]
  failed_targets = args[2]
  num_targets = args[3]
  target = target_details['name']
  cmd_runner = target_details['cmd_runner']
  env = target_details['env']
  target_profdata = target_details['profdata_file']

  logging.info('Starting target %s (completed %d/%d, of which %d succeeded)',
               target,
               len(verified_fuzzer_targets) + len(failed_targets), num_targets,
               len(verified_fuzzer_targets))

  res = _run_full_corpus(target_details) or _run_corpus_in_chunks(
      target_details)
  corpus_files = _get_target_corpus_files(target_details)
  if not res and corpus_files:
    res = _run_testcases(target, cmd_runner, env,
                         corpus_files[:INDIVIDUAL_TESTCASES_MAX_TO_TRY],
                         target_profdata)

  if res:
    verified_fuzzer_targets.append(target)
  else:
    failed_targets.append(target)

  logging.info('Finishing target %s (completed %d/%d, of which %d succeeded)',
               target,
               len(verified_fuzzer_targets) + len(failed_targets), num_targets,
               len(verified_fuzzer_targets))


def _parse_command_arguments():
  """Adds and parses relevant arguments for tool commands.

  Returns:
    A dictionary representing the arguments.
  """
  arg_parser = argparse.ArgumentParser()
  arg_parser.usage = __doc__

  arg_parser.add_argument(
      '--fuzzer-binaries-dir',
      required=True,
      type=str,
      help='Directory where the fuzzer binaries have been built.')

  arg_parser.add_argument(
      '--fuzzer-corpora-dir',
      required=True,
      type=str,
      help='Directory into which corpora have been downloaded.')

  arg_parser.add_argument('--profdata-outdir',
                          required=True,
                          type=str,
                          help='Directory where profdata will be stored.')

  arg_parser.add_argument('--fuzzer',
                          choices=ALL_FUZZER_TYPES,
                          default=LIBFUZZER,
                          help='The type of fuzzer tests to run.')

  arg_parser.add_argument
  args = arg_parser.parse_args()
  return args


def _get_all_target_details(args):
  incomplete_targets = []
  all_target_details = []

  centipede_target_binpath = os.path.join(args.fuzzer_binaries_dir, "centipede")
  if args.fuzzer == CENTIPEDE:
    if not os.path.isfile(centipede_target_binpath):
      logging.warning('%s does not exist.', centipede_target_binpath)
      return []

  for fuzzer_target in os.listdir(args.fuzzer_corpora_dir):
    fuzzer_target_binpath = os.path.join(args.fuzzer_binaries_dir,
                                         fuzzer_target)
    fuzzer_target_corporadir = os.path.join(args.fuzzer_corpora_dir,
                                            fuzzer_target)

    if not (os.path.isfile(fuzzer_target_binpath)
            and os.path.isdir(fuzzer_target_corporadir)):
      logging.warning(
          'Could not find binary file for %s, or, the provided corpora path is '
          'not a directory', fuzzer_target)
      incomplete_targets.append(fuzzer_target)
    else:
      env = dict()
      if 'DISPLAY' in os.environ:
        # Inherit X settings from the real environment
        env['DISPLAY'] = os.environ['DISPLAY']
      # This is necessary because some of our fuzzers are having redefinitions
      # due to some dependencies redefining symbols.
      env['ASAN_OPTIONS'] = 'detect_odr_violation=0'
      if args.fuzzer == CENTIPEDE:
        cmd = CentipedeRunner(centipede_path=centipede_target_binpath,
                              fuzz_target_path=fuzzer_target_binpath)
      else:  # libfuzzer
        cmd = CmdRunner(
            [fuzzer_target_binpath, '-runs=0', '-rss_limit_mb=8192'])
      all_target_details.append({
          'name':
          fuzzer_target,
          'profdata_file':
          os.path.join(REPORT_DIR, fuzzer_target + ".profdata"),
          'env':
          env,
          # RSS limit 8GB. Some of our fuzzers which involve running significant
          # chunks of Chromium code require more than the 2GB default.
          'cmd_runner':
          cmd,
          'corpus':
          fuzzer_target_corporadir,
          'files':
          '*'
      })

  # We also want to run ./chrome without a valid X server.
  # It will almost immediately exit.
  # This runs essentially no Chrome code, so will result in all the lines
  # of code in the Chrome binary being marked as 0 in the code coverage
  # report. Without doing this step, many of the files of Chrome source
  # code simply don't appear in the coverage report at all.
  chrome_target_binpath = os.path.join(args.fuzzer_binaries_dir, "chrome")
  if not os.path.isfile(chrome_target_binpath):
    logging.warning('Could not find binary file for Chrome itself')
  else:
    profraw_file = chrome_target_binpath + ".profraw"

    env = {'DISPLAY': 'not-a-real-display'}
    all_target_details.append({
        'name':
        "chrome",
        'profdata_file':
        os.path.join(REPORT_DIR, "chrome.profdata"),
        'env':
        env,
        'cmd_runner':
        ChromeRunner([chrome_target_binpath]),
        'corpus':
        None,
        'files':
        None
    })
  logging.warning("Incomplete targets (couldn't find binary): %s",
                  incomplete_targets)
  return all_target_details


def _get_fuzzilli_target_details(args):
  all_target_details = []
  fuzzer_target_binpath = os.path.join(args.fuzzer_binaries_dir, 'd8')
  source_dir = os.path.abspath(os.path.join(args.fuzzer_binaries_dir, '../../'))
  if not os.path.isfile(fuzzer_target_binpath):
    logging.warning('Could not find binary file: %s', fuzzer_target_binpath)
    return all_target_details

  for corpora_dir in os.listdir(args.fuzzer_corpora_dir):
    target_corpora_dir = os.path.join(args.fuzzer_corpora_dir, corpora_dir)
    if not os.path.isdir(target_corpora_dir):
      continue
    # for each corpora dir, the json file containing the command line args is at
    # x/fuzzdir/settings.json. Javascript files are at x/fuzzdir/corpus
    path_to_settings = os.path.join(target_corpora_dir, 'fuzzdir',
                                    'settings.json')
    with open(path_to_settings, 'r') as fp:
      settings = json.load(fp)
    path_to_js_dir = os.path.join(target_corpora_dir, 'fuzzdir', 'corpus')
    jsfiles = [
        os.path.join(path_to_js_dir, file)
        for file in os.listdir(path_to_js_dir) if file.endswith('.js')
    ]
    files_per_chunk = 80
    num_of_chunks = math.ceil(len(jsfiles) / files_per_chunk)
    for i in range(num_of_chunks):
      chunk = jsfiles[files_per_chunk * i:files_per_chunk * (i + 1)]
      all_target_details.append({
          'name':
          f'{corpora_dir}_{i}',
          'profdata_file':
          os.path.join(REPORT_DIR, f'{corpora_dir}_{i}.profdata'),
          'env':
          dict(),
          'cmd_runner':
          FuzzilliRunner(d8_path=fuzzer_target_binpath,
                         target_arguments=settings['processArguments'],
                         source_dir=source_dir),
          'corpus':
          None,
          'files':
          chunk
      })
  return all_target_details


def main():
  logging.basicConfig(format='%(asctime)s %(message)s', level=logging.INFO)
  args = _parse_command_arguments()

  # First, we make sure the report directory exists.
  pathlib.Path(REPORT_DIR).mkdir(parents=True, exist_ok=True)

  verified_fuzzer_targets = Manager().list()
  failed_targets = Manager().list()
  all_target_details = []

  if not (os.path.isfile(LLVM_PROFDATA)):
    logging.warning('No valid llvm_profdata at %s', LLVM_PROFDATA)
    exit(2)

  if not (os.path.isdir(args.profdata_outdir)):
    logging.warning('%s does not exist or is not a directory',
                    args.profdata_outdir)
    exit(2)

  if args.fuzzer == FUZZILLI:
    all_target_details = _get_fuzzilli_target_details(args)
  else:
    all_target_details = _get_all_target_details(args)

  # Run the fuzzers in parallel.
  num_cpus = int(cpu_count())
  num_targets = len(all_target_details)
  logging.info('Running %d fuzzers across %d CPUs', num_targets, num_cpus)
  with Pool(num_cpus) as p:
    results = p.map(
        _run_fuzzer_target,
        [(target_details, verified_fuzzer_targets, failed_targets, num_targets)
         for target_details in all_target_details])

  logging.info('Successful targets: %s', verified_fuzzer_targets)
  logging.info('Failed targets: %s', failed_targets)

  logging.info('Finished getting coverage information. Copying to %s',
               args.profdata_outdir)
  for fuzzer in verified_fuzzer_targets:
    cmd = [
        'cp',
        os.path.join(REPORT_DIR, fuzzer + '.profdata'), args.profdata_outdir
    ]
    logging.info(cmd)
    try:
      subprocess.check_call(cmd)
    except:
      logging.warning('Warning: failed to copy profdata for %s', fuzzer)


if __name__ == '__main__':
  sys.exit(main())
