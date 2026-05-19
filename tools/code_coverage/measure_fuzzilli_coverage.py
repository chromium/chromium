#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Run fuzzilli for the specified duration and generate a coverage report

  * Example usage: vpython3 measure_fuzzilli_coverage.py
                   --build-out-dir ~/chromium/src/out/Fuzzilli
                   --fuzzilli-dir ~/fuzzilli
                   --report-out-dir ~/chromium/src/out/report
                   --profile mojoLockManager
                   --minutes 20

    Optionally, use --ignore-filename-regex to provide a regular
    expression matching all the file paths to be excluded from the report.
    Use --filters to explicitly state all the directories or files to get
    include in the coverage report.

    The following gn args, for use with `js_in_process_fuzzer`, are a mix
    of args required for coverage and args that improve fuzzing either by
    speed up or increased coverage:
        use_sanitizer_coverage = true
        is_component_build = false
        is_debug = false
        symbol_level = 2
        blink_symbol_level = 0
        use_remoteexec = true
        dcheck_always_on = false
        is_asan = false
        use_chromium_fuzzilli = true
        v8_fuzzilli = true
        v8_static_library = true
        v8_dcheck_always_on = true
        optimize_for_fuzzing = false
        enable_mojom_fuzzer = true
        use_clang_coverage = true
"""

import argparse
import glob
import logging
from pathlib import Path
import os
import signal
import subprocess
import sys
import tempfile

DEFAULT_FUZZILLI_FLAGS = [
    '--storagePath=/tmp/fuzzilli_storage', '--overwrite', '--engine=hybrid'
]
SCRIPT_DIR = Path(__file__).resolve().parent
SRC_DIR = SCRIPT_DIR.parents[1]


class ProcessGroupHandler:
  """Simple class for handling process groups across the program.

     Notably, this class ensures that the associated process group
     is killed upon interrupt by providing an interrupt_handler.
  """

  def __init__(self):
    self.process_group = None

  def SetProcessGroup(self, pg):
    self.process_group = pg

  def KillProcessGroup(self):
    """Kill the process group with a SIGTERM signal. Grant 10 seconds
       to allow for graceful shutdown, following up with a SIGKILL if
       the process group is still active.
    """

    os.killpg(os.getpgid(self.process_group.pid), signal.SIGTERM)
    try:
      self.process_group.wait(timeout=10)
    except subprocess.TimeoutExpired:
      # Kill the process if still active
      os.killpg(os.getpgid(self.process_group.pid), signal.SIGKILL)

  def interrupt_handler(self, sig, frame):
    """Gracefully handle interrupt by killing children processes.

       When running Fuzzilli, many child processes are created. Without
       killing the entire process groups, these processes become orphan
       processes.
    """

    self.KillProcessGroup()
    sys.exit(0)


def _ParseCommandArguments():
  """Adds and parses relevant arguments for tool commands.

  Returns:
    An argparse.Namespace object representing the arguments.
  """
  arg_parser = argparse.ArgumentParser()
  arg_parser.usage = __doc__

  arg_parser.add_argument(
      '-fd',
      '--fuzzilli-dir',
      type=str,
      required=True,
      help='The absolute path to the directory containing Fuzzilli.')

  arg_parser.add_argument(
      '-b',
      '--build-out-dir',
      type=str,
      required=True,
      help='The absolute path to the Chrome output directory.')

  arg_parser.add_argument(
      '-r',
      '--report-out-dir',
      type=str,
      required=True,
      help='The absolute path to the directory to which to output the report.')

  arg_parser.add_argument(
      '-m',
      '--minutes',
      type=int,
      required=True,
      help='The number of minutes for which to run Fuzzilli.')

  arg_parser.add_argument(
      '-f',
      '--filters',
      action='append',
      required=False,
      help='Directories or files to get code coverage for, and all files under '
      'the directories are included recursively.')

  arg_parser.add_argument(
      '-i',
      '--ignore-filename-regex',
      type=str,
      help='Skip source code files with file paths that match the given '
      'regular expression. For example, use -i=\'.*/out/.*|.*/third_party/.*\' '
      'to exclude files in third_party/ and out/ folders from the report.')

  arg_parser.add_argument(
      '-p',
      '--profile',
      type=str,
      required=True,
      help='Provide a profile for Fuzzilli to use while running, identifying'
      'by the key defined in Fuzzilli\'s `profiles` Dictionary.')

  args = arg_parser.parse_args()
  return args


def _RunFuzzilli(pg_handler, fuzzilli_dir, build_out_dir, minutes, profile,
                 profraw_dir):
  """Builds and runs Fuzzilli.

     Bypasses the 'swift run' wrapper to ensure signals are handled correctly.
     Launches a process group in order to kill all associated processes
     after running for the desired duration.
  """

  os.environ["LLVM_PROFILE_FILE"] = \
    os.path.join(profraw_dir, "fuzzilli.%4m%c.profraw")

  build_command = ['swift', 'build', '-c', 'release']
  try:
    subprocess.run(build_command, check=True, cwd=fuzzilli_dir)
  except (subprocess.CalledProcessError, FileNotFoundError) as e:
    logging.fatal(e)
    return False

  # Fuzzilli strips executed programs of any environment variables, setting
  # only the environment variables specified in the optionally provided
  # profile. To ensure that js_in_process_fuzzer can see the environment
  # variable, create a temporary script for Fuzzilli to execute, in which the
  # environment variable is set.
  temp_wrapper = tempfile.NamedTemporaryFile(mode='w+',
                                             delete=False,
                                             suffix='.sh')
  try:
    temp_wrapper.write(
        '#!/bin/bash\n'
        f'export LLVM_PROFILE_FILE="{os.environ["LLVM_PROFILE_FILE"]}"\n'
        f'exec {os.path.join(build_out_dir, "js_in_process_fuzzer")} "$@"')
    temp_wrapper.flush()
    temp_wrapper.close()

    # mark wrapper as executable
    os.chmod(temp_wrapper.name, 0o755)
    print(f"The temporary file is located at: {temp_wrapper.name}")

    fuzzilli_executable = os.path.join(fuzzilli_dir,
                                       '.build/release/FuzzilliCli')
    run_command = [
        fuzzilli_executable, *DEFAULT_FUZZILLI_FLAGS, f'--profile={profile}',
        temp_wrapper.name
    ]

    # Use a process group, as this command will create many child processes that
    # continue to live if only the parent is killed
    p = subprocess.Popen(run_command, cwd=fuzzilli_dir, start_new_session=True)
    pg_handler.SetProcessGroup(p)
    try:
      timeout = minutes * 60
      p.wait(timeout=timeout)
      if p.returncode != 0:
        return False
    except subprocess.TimeoutExpired:
      # Kill the entire process group to ensure no orphan child processes
      pg_handler.KillProcessGroup()
  finally:
    if os.path.exists(temp_wrapper.name):
      os.remove(temp_wrapper.name)

  return True


def _GenerateCoverageReport(build_out_dir, report_out_dir,
                            ignore_filename_regex, filters, profraw_dir):
  if filters is None:
    filters = []

  try:
    # Create the indexed profile to pass into `coverage.py`
    #
    # glob.glob expands the wildcard
    gen_profile_command = [
        'llvm-profdata', 'merge', '-o',
        os.path.join(profraw_dir, 'coverage.profdata')
    ] + glob.glob(os.path.join(profraw_dir, '*.profraw'))
    subprocess.run(gen_profile_command, check=True)

    coverage_command = [
        f'{SRC_DIR}/tools/code_coverage/coverage.py', 'js_in_process_fuzzer',
        '-b', build_out_dir, '-o', report_out_dir, '-p',
        os.path.join(profraw_dir, 'coverage.profdata'), '--no-component-view'
    ]
    for f in filters:
      coverage_command.extend(['-f', f])
    if ignore_filename_regex:
      coverage_command.extend(['-i', ignore_filename_regex])
    subprocess.run(coverage_command, check=True)
  except FileNotFoundError as e:
    logging.fatal(e)
    return False
  except subprocess.CalledProcessError:
    # Don't log here to avoid duplicate logs (as both llvm-profdata and
    # coverage.py already log).
    return False

  return True


def Main():
  """Execute script."""

  pg_handler = ProcessGroupHandler()
  signal.signal(signal.SIGINT, pg_handler.interrupt_handler)

  args = _ParseCommandArguments()

  # Create a unique directory for storing the generated *.profraw files
  with tempfile.TemporaryDirectory() as profraw_dir:
    profraw_path = Path(profraw_dir)
    print(f'Using {profraw_path} for profraw files.')

    if not _RunFuzzilli(pg_handler, args.fuzzilli_dir, args.build_out_dir,
                        args.minutes, args.profile, profraw_path):
      sys.exit('Error: Fuzzilli failed to build or run.')

    if not _GenerateCoverageReport(args.build_out_dir, args.report_out_dir,
                                   args.ignore_filename_regex, args.filters,
                                   profraw_path):
      sys.exit('Error: Failed to generate coverage report.')


if __name__ == '__main__':
  sys.exit(Main())
