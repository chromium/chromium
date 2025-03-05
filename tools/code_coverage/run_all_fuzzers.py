# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Run all Chromium libfuzzer targets that have corresponding corpora,
   then save the profdata files.

  * Example usage: run_all_fuzzers.py --fuzzer-binaries-dir foo
                   --fuzzer-corpora-dir bar --profdata-outdir baz
"""

import argparse
import glob
import json
import math
import os
import subprocess
import sys

from multiprocessing import Process, Manager, cpu_count, Pool
from typing import Mapping, Sequence

WHOLE_CORPUS_RETRIES = 2
WHOLE_CORPUS_TIMEOUT_SECS = 1200
INDIVIDUAL_TESTCASE_TIMEOUT_SECS = 60
INDIVIDUAL_TESTCASES_MAX_TO_TRY = 500
INDIVIDUAL_TESTCASES_SUCCESSES_NEEDED = 100

LIBFUZZER = 'libfuzzer'
CENTIPEDE = 'centipede'
FUZZILLI = 'fuzzilli'
ALL_FUZZER_TYPES = [LIBFUZZER, CENTIPEDE, FUZZILLI]
REPORT_DIR = 'out/report'

LLVM_PROFDATA = 'third_party/llvm-build/Release+Asserts/bin/llvm-profdata'


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
    print("profdata merge failed, treating this target as failed")
  finally:
    for f in inputs:
      if os.path.exists(f):
        os.unlink(f)
  return False


def _run_and_log(cmd: Sequence[str], env: Mapping[str, str], timeout: float,
                 annotation: str) -> bool:
  """Runs a given command and logs output in case of failure.

  Args:
    cmd: the command and its arguments.
    env: environment variables to apply.
    timeout: the timeout to apply, in seconds.
    annotation: annotation to add to logging.

  Returns:
    True iff the command ran successfully.
  """
  print(f"Trying command: {cmd} ({annotation})")
  try:
    subprocess.run(cmd,
                   env=env,
                   timeout=timeout,
                   capture_output=True,
                   check=True)
    return True
  except Exception as e:
    if type(e) == subprocess.TimeoutExpired:
      print(f"Command {cmd!s} ({annotation}) timed out " +
            f"after {e.timeout!s} seconds")
    else:
      print(f"Command {cmd!s} ({annotation}) return code: " +
            f"{e.returncode!s}\nStdout:\n{e.output}\nStderr:\n{e.stderr}")
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
  cmd = target_details['cmd']
  env = target_details['env']
  corpus_dir = target_details['corpus']
  corpus_files = target_details['files']
  profraw_dir = target_details['profraw_dir']
  target_profdata = target_details['profdata_file']

  print("Starting target %s (completed %d/%d, of which %d succeeded)" %
        (target, len(verified_fuzzer_targets) + len(failed_targets),
         num_targets, len(verified_fuzzer_targets)))

  fullcorpus_profraw = os.path.join(profraw_dir, target + "_%p.profraw")
  env['LLVM_PROFILE_FILE'] = fullcorpus_profraw
  fullcorpus_cmd = cmd.copy()
  if corpus_files not in [None, '*']:
    # Fuzzilli's case
    jsfiles = corpus_files.split()
    fullcorpus_cmd.extend([os.path.join(corpus_dir, file) for file in jsfiles])
  _erase_profraws(fullcorpus_profraw)
  for i in range(WHOLE_CORPUS_RETRIES):
    ok = _run_and_log(fullcorpus_cmd, env, WHOLE_CORPUS_TIMEOUT_SECS,
                      f"full corpus attempt {i}")
    if ok:
      break

  valid_profiles = 0
  matching_profraws = list(_matching_profraws(fullcorpus_profraw))
  # There may be several if the fuzzer involved multiple processes,
  # e.g. a fuzztest with a wrapper executable
  ok = _profdata_merge(matching_profraws, target_profdata)
  if ok:
    valid_profiles = 1

  if valid_profiles == 0 and corpus_files is not None:
    # We failed to run the fuzzer with the whole corpus in one go. That probably
    # means one of the test cases caused a crash. Let's run each test
    # case one at a time. The resulting profraw files can be hundreds of MB
    # each so after each test case, we merge them into an accumulated
    # profdata file.
    if corpus_files == '*':
      corpus_files = os.listdir(corpus_dir)
    else:
      corpus_files = corpus_files.split()

    for count, corpus_entry in enumerate(corpus_files):
      specific_test_case_profraw = os.path.join(
          profraw_dir, target + "_" + str(count) + "_%p.profraw")
      test_case = os.path.join(corpus_dir, corpus_entry)
      specific_test_case_cmd = cmd + [test_case]
      env['LLVM_PROFILE_FILE'] = specific_test_case_profraw
      _erase_profraws(specific_test_case_profraw)
      _run_and_log(specific_test_case_cmd, env,
                   INDIVIDUAL_TESTCASE_TIMEOUT_SECS,
                   f"specific test case {count}")
      resulting_profraws = list(_matching_profraws(specific_test_case_profraw))
      if resulting_profraws:
        # We recorded valid profraws, let's merge them into
        # the accumulating profdata
        valid_profiles += 1
        temp_profdata = os.path.join(profraw_dir,
                                     target + "_accumlated.profraw")
        if os.path.exists(target_profdata):
          os.rename(target_profdata, temp_profdata)
          resulting_profraws.append(temp_profdata)
        ok = _profdata_merge(resulting_profraws, target_profdata)
        if not ok:
          valid_profiles = 0
          break
      # The corpus may be huge - don't keep going forever.
      if count > INDIVIDUAL_TESTCASES_MAX_TO_TRY:
        print(f"Skipping remaining test cases for {target} - >" +
              f"{INDIVIDUAL_TESTCASES_MAX_TO_TRY} tried")
        break
      # And if we've got enough valid coverage files, assume this is a
      # reasonable approximation of the total coverage. This is partly
      # to ensure the profdata command line isn't too huge, partly
      # to reduce processing time to something reasonable, and partly
      # because profraw files are huge and can fill up bot disk space.
      if valid_profiles > INDIVIDUAL_TESTCASES_SUCCESSES_NEEDED:
        print(
            f"Skipping remaining test cases for {target}, >%" +
            f"{INDIVIDUAL_TESTCASES_SUCCESSES_NEEDED} valid profiles recorded.")
        break
  if valid_profiles == 0:
    failed_targets.append(target)
    return
  verified_fuzzer_targets.append(target)
  print("Finishing target %s (completed %d/%d, of which %d succeeded)" %
        (target, len(verified_fuzzer_targets) + len(failed_targets),
         num_targets, len(verified_fuzzer_targets)))


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

  for fuzzer_target in os.listdir(args.fuzzer_corpora_dir):
    fuzzer_target_binpath = os.path.join(args.fuzzer_binaries_dir,
                                         fuzzer_target)
    fuzzer_target_corporadir = os.path.join(args.fuzzer_corpora_dir,
                                            fuzzer_target)

    if not (os.path.isfile(fuzzer_target_binpath)
            and os.path.isdir(fuzzer_target_corporadir)):
      print((
          'Could not find binary file for %s, or, the provided corpora path is '
          'not a directory') % fuzzer_target)
      incomplete_targets.append(fuzzer_target)
    else:
      env = dict()
      if 'DISPLAY' in os.environ:
        # Inherit X settings from the real environment
        env['DISPLAY'] = os.environ['DISPLAY']
      all_target_details.append({
          'name':
          fuzzer_target,
          'profraw_dir':
          REPORT_DIR,
          'profdata_file':
          os.path.join(REPORT_DIR, fuzzer_target + ".profdata"),
          'env':
          env,
          # RSS limit 8GB. Some of our fuzzers which involve running significant
          # chunks of Chromium code require more than the 2GB default.
          'cmd': [
              fuzzer_target_binpath, '-runs=0', '-rss_limit_mb=8192',
              fuzzer_target_corporadir
          ],
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
    print('Could not find binary file for Chrome itself')
  else:
    profraw_file = chrome_target_binpath + ".profraw"

    env = {'DISPLAY': 'not-a-real-display'}
    all_target_details.append({
        'name':
        "chrome",
        'profraw_dir':
        REPORT_DIR,
        'profdata_file':
        os.path.join(REPORT_DIR, "chrome.profdata"),
        'env':
        env,
        'cmd': [chrome_target_binpath],
        'corpus':
        None,
        'files':
        None
    })
  print("Incomplete targets (couldn't find binary): %s" % incomplete_targets)
  return all_target_details


def _get_fuzzilli_target_details(args):
  all_target_details = []
  fuzzer_target_binpath = os.path.join(args.fuzzer_binaries_dir, 'd8')
  if not os.path.isfile(fuzzer_target_binpath):
    print(f'Could not find binary file: {fuzzer_target_binpath}')
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
    cmd = [fuzzer_target_binpath]
    cmd.extend(settings['processArguments'])
    path_to_js_dir = os.path.join(target_corpora_dir, 'fuzzdir', 'corpus')
    jsfiles = [
        file for file in os.listdir(path_to_js_dir) if file.endswith('.js')
    ]
    files_per_chunk = 10
    num_of_chunks = math.ceil(len(jsfiles) / files_per_chunk)
    for i in range(num_of_chunks):
      chunk = jsfiles[files_per_chunk * i:files_per_chunk * (i + 1)]
      all_target_details.append({
          'name':
          f'{corpora_dir}_{i}',
          'profraw_dir':
          REPORT_DIR,
          'profdata_file':
          os.path.join(REPORT_DIR, f'{corpora_dir}_{i}.profdata'),
          'env':
          dict(),
          'cmd':
          cmd,
          'corpus':
          path_to_js_dir,
          'files':
          ' '.join(chunk)
      })
  return all_target_details


def main():
  args = _parse_command_arguments()

  verified_fuzzer_targets = Manager().list()
  failed_targets = Manager().list()
  all_target_details = []

  if not (os.path.isfile(LLVM_PROFDATA)):
    print('No valid llvm_profdata at %s' % LLVM_PROFDATA)
    exit(2)

  if not (os.path.isdir(args.profdata_outdir)):
    print('%s does not exist or is not a directory' % args.profdata_outdir)
    exit(2)

  if args.fuzzer == FUZZILLI:
    all_target_details = _get_fuzzilli_target_details(args)
  else:
    all_target_details = _get_all_target_details(args)

  # Run the fuzzers in parallel.
  num_cpus = int(cpu_count())
  num_targets = len(all_target_details)
  print("Running %d fuzzers across %d CPUs" % (num_targets, num_cpus))
  with Pool(num_cpus) as p:
    results = p.map(
        _run_fuzzer_target,
        [(target_details, verified_fuzzer_targets, failed_targets, num_targets)
         for target_details in all_target_details])

  print("Successful targets: %s" % verified_fuzzer_targets)
  print("Failed targets: %s" % failed_targets)

  print("Finished getting coverage information. Copying to %s" %
        args.profdata_outdir)
  for fuzzer in verified_fuzzer_targets:
    cmd = [
        'cp',
        os.path.join(REPORT_DIR, fuzzer + '.profdata'), args.profdata_outdir
    ]
    print(cmd)
    try:
      subprocess.check_call(cmd)
    except:
      print.warning("Warning: failed to copy profdata for %s" % fuzzer)


if __name__ == '__main__':
  sys.exit(main())
