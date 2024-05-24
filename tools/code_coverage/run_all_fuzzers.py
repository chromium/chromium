# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Run all Chromium libfuzzer targets that have corresponding corpora,
   then save the profdata files.

  * Example usage: run_all_fuzzers.py --fuzzer-binaries-dir foo
                   --fuzzer-corpora-dir bar --profdata-outdir baz
"""

import argparse
from multiprocessing import Process, Manager, cpu_count, Pool
import os
import subprocess
import glob
from typing import Mapping, Sequence

WHOLE_CORPUS_RETRIES = 2
WHOLE_CORPUS_TIMEOUT_SECS = 1200
INDIVIDUAL_TESTCASE_TIMEOUT_SECS = 60
INDIVIDUAL_TESTCASES_MAX_TO_TRY = 500
INDIVIDUAL_TESTCASES_SUCCESSES_NEEDED = 100


def _profdata_merge(inputs: Sequence[str], output: str) -> bool:
  """Merges the given profraw files into a single file.

  Deletes any inputs, whether or not it succeeded.

  Args:
    inputs: paths to input files.
    output: output file path.

  Returns:
    True if it worked.
  """
  llvm_profdata_cmd = [llvm_profdata, 'merge', '-sparse'
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
      print(
          f"Command {cmd!s} ({annotation}) timed out after {e.timeout!s} seconds"
      )
    else:
      print(
          f"Command {cmd!s} ({annotation}) return code: {e.returncode!s}\nStdout:\n{e.output}\nStderr:\n{e.stderr}"
      )
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
  profraw_dir = target_details['profraw_dir']
  target_profdata = target_details['profdata_file']

  print("Starting target %s (completed %d/%d, of which %d succeeded)" %
        (target, len(verified_fuzzer_targets) + len(failed_targets),
         num_targets, len(verified_fuzzer_targets)))

  fullcorpus_profraw = os.path.join(profraw_dir, target + "_%p.profraw")
  env['LLVM_PROFILE_FILE'] = fullcorpus_profraw
  fullcorpus_cmd = cmd.copy()
  if corpus_dir is not None:
    fullcorpus_cmd.append(corpus_dir)
  _erase_profraws(fullcorpus_profraw)
  for i in range(WHOLE_CORPUS_RETRIES):
    ok = _run_and_log(fullcorpus_cmd, env, WHOLE_CORPUS_TIMEOUT_SECS,
                      f"full corpus attempt {i}")
    if ok:
      break
  valid_profiles = 0
  for profraw in _matching_profraws(fullcorpus_profraw):
    ok = _profdata_merge([profraw], target_profdata)
    if ok:
      valid_profiles = 1
  if valid_profiles == 0:
    # We failed to run the fuzzer with the whole corpus in one go. That probably
    # means one of the test cases caused a crash. Let's run each test
    # case one at a time. The resulting profraw files can be hundreds of MB
    # each so after each test case, we merge them into an accumulated
    # profdata file.
    for count, corpus_entry in enumerate(os.listdir(corpus_dir)):
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
        print(
            f"Skipping remaining test cases for {target} - >{INDIVIDUAL_TESTCASES_MAX_TO_TRY} tried"
        )
        break
      # And if we've got enough valid coverage files, assume this is a
      # reasonable approximation of the total coverage. This is partly
      # to ensure the profdata command line isn't too huge, partly
      # to reduce processing time to something reasonable, and partly
      # because profraw files are huge and can fill up bot disk space.
      if valid_profiles > INDIVIDUAL_TESTCASES_SUCCESSES_NEEDED:
        print(
            f"Skipping remaining test cases for {target}, >%{INDIVIDUAL_TESTCASES_SUCCESSES_NEEDED} valid profiles recorded."
        )
        break
  if valid_profiles == 0:
    failed_targets.append(target)
    return
  verified_fuzzer_targets.append(target)
  print("Finishing target %s (completed %d/%d, of which %d succeeded)" %
        (target, len(verified_fuzzer_targets) + len(failed_targets),
         num_targets, len(verified_fuzzer_targets)))



def _ParseCommandArguments():
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

  arg_parser.add_argument
  args = arg_parser.parse_args()
  return args


args = _ParseCommandArguments()

incomplete_targets = []
verified_fuzzer_targets = Manager().list()
failed_targets = Manager().list()
reportdir = 'out/report'
all_target_details = []
llvm_profdata = 'third_party/llvm-build/Release+Asserts/bin/llvm-profdata'

if not (os.path.isfile(llvm_profdata)):
  print('No valid llvm_profdata at %s' % llvm_profdata)
  exit(2)

if not (os.path.isdir(args.profdata_outdir)):
  print('%s does not exist or is not a directory' % args.profdata_oudir)
  exit(2)

for fuzzer_target in os.listdir(args.fuzzer_corpora_dir):
  fuzzer_target_binpath = os.path.join(args.fuzzer_binaries_dir, fuzzer_target)
  fuzzer_target_corporadir = os.path.join(args.fuzzer_corpora_dir,
                                          fuzzer_target)

  if not (os.path.isfile(fuzzer_target_binpath)
          and os.path.isdir(fuzzer_target_corporadir)):
    print(
        ('Could not find binary file for %s, or, the provided corpora path is '
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
        reportdir,
        'profdata_file':
        os.path.join(reportdir, fuzzer_target + ".profdata"),
        'env':
        env,
        # RSS limit 8GB. Some of our fuzzers which involve running significant
        # chunks of Chromium code require more than the 2GB default.
        'cmd': [fuzzer_target_binpath, '-runs=0', '-rss_limit_mb=8192'],
        'corpus':
        fuzzer_target_corporadir
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
  profraw_path = os.path.join(reportdir, profraw_file)

  env = {'DISPLAY': 'not-a-real-display'}
  all_target_details.append({
      'name':
      "chrome",
      'profraw_dir':
      reportdir,
      'profdata_file':
      os.path.join(reportdir, "chrome.profdata"),
      'env':
      env,
      'cmd': [chrome_target_binpath],
      'corpus':
      None
  })

# Run the fuzzers in parallel.
cpu_count = int(cpu_count())
num_targets = len(all_target_details)
print("Running %d fuzzers across %d CPUs" % (num_targets, cpu_count))
with Pool(cpu_count) as p:
  results = p.map(
      _run_fuzzer_target,
      [(target_details, verified_fuzzer_targets, failed_targets, num_targets)
       for target_details in all_target_details])

print("Successful targets: %s" % verified_fuzzer_targets)
print("Failed targets: %s" % failed_targets)
print("Incomplete targets (couldn't find binary): %s" % incomplete_targets)

print("Finished getting coverage information. Copying to %s" %
      args.profdata_outdir)
for fuzzer in verified_fuzzer_targets:
  cmd = [
      'cp',
      os.path.join(reportdir, fuzzer + '.profdata'), args.profdata_outdir
  ]
  print(cmd)
  try:
    subprocess.check_call(cmd)
  except:
    print.warning("Warning: failed to copy profdata for %s" % fuzzer)
