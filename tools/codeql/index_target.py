# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" Script for building a Chromium CodeQL database."""
import argparse
import functools
import json
import multiprocessing
import subprocess
import logging
import time
import os
import traceback
import gn_sources_tools
import targets_to_index

from collections import namedtuple

CommandOutput = namedtuple("CommandOutput", "output traceback")


def log_subprocess_output(output, logger=None):
  """ Reads from a subprocess's stdout and writes it to `logger`,
  or, if none given, to stdout.  """
  if not logger:
    print(output)
  else:
    logger.info(output)


class CodeQLDatabase:

  def __init__(self, src_path, db_path, codeql_binary_path):
    """ Construct a new `CodeQLDatabase` object.
    :param src_path: The path to the chromium/src tree.
    :param db_path: The path where the CodeQL database will be created.
    :return: returns nothing
    """
    self.db_path = db_path
    try:
      process_stdout = subprocess.check_output([
          codeql_binary_path, 'database', 'init', f'--source-root={src_path}',
          '--language=cpp', db_path, '--overwrite'
      ])
      log_subprocess_output(process_stdout)
    except subprocess.CalledProcessError:
      # Presumably failed due to an invalid value for db_path.
      raise ValueError


def index_one_target(target_name,
                     src_path,
                     db_path,
                     codeql_binary_path,
                     out_path,
                     logger,
                     ninja_path='ninja',
                     gn_path='gn',
                     logfile=None,
                     reduce_cores_used=False):
  try:
    process_stdout = subprocess.check_output([gn_path, 'clean', out_path])
    log_subprocess_output(process_stdout)
  except subprocess.CalledProcessError as e:
    print("Failed to clean build directory between targets")
    print("stdout: %s" % e.stdout)
    print("stderr: %s" % e.stderr)
    exit(1)
  db_path = os.path.join(db_path, target_name)
  os.mkdir(os.path.join(db_path))

  start_time = time.time()

  print("Initializing codeql.")
  codeql_db = ""
  try:
    codeql_db = CodeQLDatabase(src_path, db_path, codeql_binary_path)
  except ValueError:
    print("Could not initialize CodeQL database at %s" % db_path)
    exit(1)

  print("Tracing compilation.")
  trace_command = [
      codeql_binary_path, 'database', 'trace-command', db_path,
      f'--working-dir={src_path}', '--', ninja_path, '-C', out_path, target_name
  ]
  if reduce_cores_used:
    usable_cpu_count = int(multiprocessing.cpu_count() / 2)
    trace_command.extend(['-j', str(usable_cpu_count)])
  try:
    process_stdout = subprocess.check_output(trace_command)
    log_subprocess_output(process_stdout)
  except subprocess.CalledProcessError as e:
    print("CodeQL trace-process failed with return code %s" % e.returncode)
    print("stdout: %s" % e.stdout)
    print("stderr: %s" % e.stderr)
    exit(1)

  print("Finalizing codeql db.")
  try:
    process_stdout = subprocess.check_output(
        [codeql_binary_path, 'database', 'finalize', '-j=-1', db_path])
    log_subprocess_output(process_stdout)
  except subprocess.CalledProcessError as e:
    print("CodeQL DB finalization failed with return code %s" % e.returncode)
    print("stdout: %s" % e.stdout)
    print("stderr: %s" % e.stderr)
  print("Database creation complete.")
  total_time = time.time() - start_time
  print("Time elapsed:")
  print(str(total_time))


def main():
  logger = logging.getLogger('log')
  logger.setLevel(logging.INFO)
  actual_cwd = os.getcwd()
  script_directory = os.path.dirname(os.path.realpath(__file__))
  src_path = os.path.join(script_directory, '..', '..')
  if actual_cwd != os.path.normpath(src_path):
    print("Failure: Script must be executed from `chromium/src`. Exiting.")
    print(actual_cwd)
    print(src_path)
    exit(1)

  print("Parsing command line arguments.")
  parser = argparse.ArgumentParser(
      description='Build CodeQL database for Chromium browser process')
  parser.add_argument(
      '--out_path',
      '-o',
      type=str,
      default='out/release',
      help='Relative path inside chromium checkout to build directory')
  parser.add_argument('--db_path',
                      '-d',
                      type=str,
                      required=True,
                      help='Path to output database')
  parser.add_argument(
      '--logfile',
      '-l',
      type=str,
      help="absolute path to logfile for `trace` calls, if desired")
  parser.add_argument(
      '--gn_targets',
      '-g',
      action='append',
      type=str,
      help=(
          'name for the specific GN target you want a CodeQL database for '
          '(e.g. `//components:components_unittests`); if left blank, indexes '
          'everything'))
  parser.add_argument(
      '--codeql_binary_path',
      '-c',
      type=str,
      default='codeql',
      help=('Path to the codeql binary. If this is not set, the script assumes '
            'it is located at `codeql` somewhere in the user\'s PATH.'))
  parser.add_argument(
      '--gn_path',
      type=str,
      default='gn',
      help=('Path to the gn executable. If this is not set, the script assumes '
            'it is located at `gn` somehwere in the user\'s PATH.'))
  parser.add_argument(
      '--ninja_path',
      type=str,
      default='ninja',
      help=('Path to the ninja executable. If this is not set, the script '
            'assumes it is located at `ninja` somehwere in the user\'s PATH.'))
  parser.add_argument(
      '--reduce_cores_used',
      default=False,
      action='store_true',
      help=('If set, reduces the number of cores used when building a target.'))
  args = parser.parse_args()

  if (args.logfile):
    ch = logging.FileHandler(args.logfile)
    ch.setFormatter(logging.Formatter('%(message)s'))
    logger.addHandler(ch)
  src_path = os.path.abspath(os.path.expanduser(src_path))
  args.db_path = os.path.abspath(os.path.expanduser(args.db_path))

  # If an args.gn_target is given, index those targets.
  # Otherwise, index the targets in targets_to_index.
  actual_targets_to_index = []
  if not args.gn_targets:
    actual_targets_to_index = targets_to_index.full_targets
  else:
    actual_targets_to_index = args.gn_targets
  for target in actual_targets_to_index:
    index_one_target(target, src_path, args.db_path, args.codeql_binary_path,
                     args.out_path, logger, args.ninja_path, args.gn_path,
                     args.logfile, args.reduce_cores_used)

if __name__ == '__main__':
  main()
