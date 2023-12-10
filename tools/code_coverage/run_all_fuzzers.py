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

FUZZ_RETRIES = 3


def _run_fuzzer_target(args):
  """ Runs a given fuzzer target. Designed to be called in parallel.

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

  Returns:
    None.
  """
  target_details = args[0]
  verified_fuzzer_targets = args[1]
  failed_targets = args[2]
  target = target_details['name']
  cmd = target_details['cmd']
  env = target_details['env']
  corpus_dir = target_details['corpus']
  profraw_dir = target_details['profraw_dir']
  target_profdata = target_details['profdata_file']
  fullcorpus_profraw = os.path.join(profraw_dir, target + ".profraw")
  env['LLVM_PROFILE_FILE'] = fullcorpus_profraw
  fullcorpus_cmd = cmd.copy()
  if corpus_dir is not None:
    fullcorpus_cmd.append(corpus_dir)
  valid_profraws = set()
  for i in range(FUZZ_RETRIES):
    print("Trying command with full corpus %s" % str(fullcorpus_cmd))
    try:
      subprocess.run(fullcorpus_cmd,
                     env=env,
                     timeout=1800,
                     capture_output=True,
                     check=True)
      break
    except Exception as e:
      print(
          "Command %s exited with non-zero return code, failing on iteration %d"
          % (fullcorpus_cmd, i))
      if type(e) == subprocess.TimeoutExpired:
        print("Timed out after %d seconds" % e.timeout)
      else:
        print("Return code: " + str(e.returncode))
        print("**** FULL FUZZING OUTPUT BELOW ***")
        print(e.output)
        print(e.stderr)
        print("*** FULL FUZZING OUTPUT ABOVE ***")
  if os.path.exists(
      fullcorpus_profraw) and os.path.getsize(fullcorpus_profraw) > 0:
    valid_profraws.add(fullcorpus_profraw)
  if len(valid_profraws) == 0 and corpus_dir is not None:
    # We failed to run the fuzzer with the whole corpus in one go. That probably
    # means one of the test cases caused a crash. Let's run each test
    # case one at a time.
    for count, corpus_entry in enumerate(os.listdir(corpus_dir)):
      specific_test_case_profraw = os.path.join(
          profraw_dir, target + "_" + str(count) + ".profraw")
      test_case = os.path.join(corpus_dir, corpus_entry)
      specific_test_case_cmd = cmd + [test_case]
      env['LLVM_PROFILE_FILE'] = specific_test_case_profraw
      print("Trying command with specific test case %d (%s)" %
            (count, str(specific_test_case_cmd)))
      try:
        subprocess.run(specific_test_case_cmd,
                       env=env,
                       timeout=1800,
                       capture_output=True,
                       check=True)
      except Exception as e:
        print("Command %s exited with non-zero return code" %
              (specific_test_case_cmd))
        if type(e) == subprocess.TimeoutExpired:
          print("Timed out after %d seconds" % e.timeout)
        else:
          print("Return code: " + str(e.returncode))
          print("**** FULL FUZZING OUTPUT BELOW ***")
          print(e.output)
          print(e.stderr)
          print("*** FULL FUZZING OUTPUT ABOVE ***")
      if os.path.exists(specific_test_case_profraw
                        ) and os.path.getsize(specific_test_case_profraw) > 0:
        valid_profraws.add(specific_test_case_profraw)
      # The corpus may be huge - don't keep going forever.
      if count > 1000:
        print("Skipping remaining test cases - >1000 tried")
        break
      # And if we've got 10 valid coverage files, assume this is a
      # reasonable approximation of the total coverage. This is partly
      # to ensure the profdata command line isn't too huge, partly
      # to reduce processing time to something reasonable, and partly
      # because profraw files are huge and can fill up bot disk space.
      if len(valid_profraws) > 10:
        print(
            "Skipping remaining individual test cases, >10 valid profiles recorded."
        )
        break
  if len(valid_profraws) == 0:
    failed_targets.append(target)
    return
  # This script is currently run only on Linux. If in future we want to use
  # it on Windows, where command line length limits are smaller, we might
  # want to use the -f argument instead of listing them all
  llvm_profdata_cmd = [llvm_profdata, 'merge', '-sparse'
                       ] + list(valid_profraws) + ['-o', target_profdata]
  subprocess.check_call(llvm_profdata_cmd)
  # Free up disk space by deleting all non-zero-length profraw files
  for f in valid_profraws:
    os.unlink(f)
  verified_fuzzer_targets.append(target)


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
    all_target_details.append({
        'name':
        fuzzer_target,
        'profraw_dir':
        reportdir,
        'profdata_file':
        os.path.join(reportdir, fuzzer_target + ".profdata"),
        'env':
        dict(),
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
with Pool(cpu_count) as p:
  results = p.map(_run_fuzzer_target,
                  [(target_details, verified_fuzzer_targets, failed_targets)
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
