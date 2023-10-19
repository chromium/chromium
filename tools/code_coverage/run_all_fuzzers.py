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
    args[0]: Name of the fuzzer target.
    args[1]: Command to be run.
    args[2]: Environment variables.
    args[3]: A multiprocessing.Manager.list for names of successful fuzzers.
    args[4]: A multiprocessing.Manager.list for names of failed fuzzers.

  Returns:
    None.
  """
  target = args[0]
  cmd = args[1]
  env = args[2]
  verified_fuzzer_targets = args[3]
  failed_targets = args[4]
  target_profraw = os.path.join(reportdir, target + ".profraw")
  target_profdata = os.path.join(reportdir, target + ".profdata")
  for i in range(FUZZ_RETRIES):
    print("Trying command %s" % str(cmd))
    try:
      subprocess.run(cmd,
                     env=env,
                     timeout=1800,
                     capture_output=True,
                     check=True)
      break
    except Exception as e:
      print(
          "Command %s exited with non-zero return code, failing on iteration %d"
          % (cmd, i))
      if type(e) == subprocess.TimeoutExpired:
        print("Timed out after %d seconds" % e.timeout)
      else:
        print("Return code: " + str(e.returncode))
        print("**** FULL FUZZING OUTPUT BELOW ***")
        print(e.output)
        print(e.stderr)
        print("*** FULL FUZZING OUTPUT ABOVE ***")
  if not os.path.isfile(target_profraw):
    failed_targets.append(target)
    return
  llvm_profdata_cmd = [
      llvm_profdata, 'merge', '-sparse', target_profraw, '-o', target_profdata
  ]
  subprocess.check_call(llvm_profdata_cmd)
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
commands = []
targets = []
envs = []
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
    subprocess_cmd = [
        fuzzer_target_binpath, '-runs=0', fuzzer_target_corporadir
    ]
    profraw_file = fuzzer_target + ".profraw"
    profraw_path = os.path.join(reportdir, profraw_file)
    env = {'LLVM_PROFILE_FILE': profraw_path}
    targets.append(fuzzer_target)
    commands.append(subprocess_cmd)
    envs.append(env)

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
  env = {'LLVM_PROFILE_FILE': profraw_path, 'DISPLAY': 'not-a-real-display'}
  targets.append(chrome_target_binpath)
  commands.append([chrome_target_binpath])
  envs.append(env)

# Run the fuzzers in parallel.
cpu_count = int(cpu_count())
with Pool(cpu_count) as p:
  results = p.map(
      _run_fuzzer_target,
      [(target, command, env, verified_fuzzer_targets, failed_targets)
       for target, command, env in zip(targets, commands, envs)])

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
    print.warning("Warning: failed to copy profraw for %s" % fuzzer)
