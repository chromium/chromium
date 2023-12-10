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


def log_subprocess_output(output, logger=None):
  """ Reads from a subprocess's stdout and writes it to `logger`,
  or, if none given, to stdout.  """
  if not logger:
    print(output)
  else:
    logger.info(output)


def get_compilation_db(src_path, out_path):
  root_command = os.path.join(src_path,
                              'tools/clang/scripts/generate_compdb.py')
  command = [root_command]
  command.extend(['-p', out_path])
  print(f"Compilation DB command: {command}")
  output = subprocess.check_output(command, cwd=src_path)
  return json.loads(output)


def trace(processing_num, entry, *, codeql_db_path, successful_commands,
          failed_commands, logger):
  directory = entry['directory']
  command = entry['command']

  command = command.replace('\\"', '"').replace('\\(',
                                                '(').replace('\\)',
                                                             ')').split(' ')
  command[0] = os.path.abspath(os.path.join(directory, command[0]))

  try:
    subprocess.check_output([
        'codeql', 'database', 'trace-command', codeql_db_path,
        f'--working-dir={directory}', '--', *command
    ],
                            stderr=subprocess.STDOUT)
    successful_commands.append(str(command))
    logger.info("************ Upto " + str(processing_num))

  except subprocess.CalledProcessError as e:
    logger.info(
        "FAILURE: a subprocess.CalledProcessError occurred while running %s" %
        command)
    logger.info(traceback.format_exec())
    logger.info("Working directory was %s" % directory)
    failed_commands.append(str(command))


class CodeQLDatabase:
  def __init__(self, src_path, db_path):
    """ Construct a new `CodeQLDatabase` object.
    :param src_path: The path to the chromium/src tree.
    :param db_path: The path where the CodeQL database will be created.
    :return: returns nothing
    """
    self.db_path = db_path
    try:
      process_stdout = subprocess.check_output([
          'codeql', 'database', 'init', f'--source-root={src_path}',
          '--language=cpp', db_path, '--overwrite'
      ])
      log_subprocess_output(process_stdout)
    except subprocess.CalledProcessError:
      # Presumably failed due to an invalid value for db_path.
      raise ValueError


def main():
  print('BEFORE RUNNING THIS SCRIPT: Make sure you have done a *full build*'
        'in your --out_dir.')
  print('This script does not build anything itself, and will fail in strange '
        'ways if there is an empty or incomplete build!"')
  start_time = time.time()
  logger = logging.getLogger('log')
  logger.setLevel(logging.INFO)
  actual_cwd = os.getcwd()
  script_directory = os.path.dirname(os.path.realpath(__file__))
  src_path = os.path.join(script_directory, '..', '..')
  if actual_cwd != os.path.normpath(src_path):
    print("Failure: Script must be executed from `chromium/src`. Exiting.")
    print(actual_cwd)
    print(src_path)
    exit()

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
      '--gn_target',
      '-g',
      type=str,
      help=(
          'name for the specific GN target you want a CodeQL database for '
          '(e.g. `//components:components_unittests`); if left blank, indexes '
          'everything'))
  args = parser.parse_args()

  if (args.logfile):
    ch = logging.FileHandler(args.logfile)
    ch.setFormatter(logging.Formatter('%(message)s'))
    logger.addHandler(ch)
  src_path = os.path.abspath(os.path.expanduser(src_path))
  args.db_path = os.path.abspath(os.path.expanduser(args.db_path))

  print("Generating compilation db.")
  compilation_db = []

  if args.gn_target:
    print("Fetching all transitive source dependencies for " +
          str(args.gn_target))
    gn_sources_dict = gn_sources_tools.dictionary_of_all_transitive_sources(
        args.gn_target, args.out_path)
    initial_compilation_db = get_compilation_db(src_path, args.out_path)
    print('Filtering compilation DB to only include matches '
          'from GN transitive dependencies.')
    compilation_db = []
    for entry in initial_compilation_db:
      if entry['file'] in gn_sources_dict:
        compilation_db.append(entry)
  else:
    compilation_db = get_compilation_db(src_path, args.out_path)

  print("Initializing codeql.")
  codeql_db = ""
  try:
    codeql_db = CodeQLDatabase(src_path, args.db_path)
  except ValueError:
    print("Could not initialize CodeQL database at %s" % args.db_path)
    exit()

  print("Tracing compilation.")
  if (args.logfile):
    print("Progress on trace compilation will be reported to %s" % args.logfile)
  my_cpu_count = int(multiprocessing.cpu_count())
  failed_commands = multiprocessing.Manager().list()
  successful_commands = multiprocessing.Manager().list()
  with multiprocessing.Pool(my_cpu_count) as p:
    results = p.starmap(
        functools.partial(trace,
                          codeql_db_path=codeql_db.db_path,
                          successful_commands=successful_commands,
                          failed_commands=failed_commands,
                          logger=logger),
        [(num, entry) for num, entry in enumerate(compilation_db)])

  print("Successful commands: %s" % len(successful_commands))
  print("Failed commands: %s" % len(failed_commands))

  print("Finalizing codeql db.")
  try:
    process_stdout = subprocess.check_output(
        ['codeql', 'database', 'finalize', '-j=-1', args.db_path])
    log_subprocess_output(process_stdout)
  except subprocess.CalledProcessError as e:
    print("CodeQL DB finalization failed with return code %s" % e.returncode)
    print("stdout: %s" % e.stdout)
    print("stderr: %s" % e.stderr)
  print("Database creation complete.")
  total_time = time.time() - start_time
  print("Time elapsed:")
  print(str(total_time))


if __name__ == '__main__':
  main()
