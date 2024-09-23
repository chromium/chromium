#!/usr/bin/env python
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Run a single fuzz target built with code coverage instrumentation.

This script assumes that corresponding corpus was downloaded via gclient sync
and saved to: src/testing/libfuzzer/fuzzer_corpus/{fuzzer_name}/.
"""

import argparse
import glob
import json
import logging
import os
import shutil
import signal
import subprocess
import sys
import time
import zipfile

_THIS_DIR = os.path.dirname(os.path.realpath(__file__))

# Path to the fuzzer corpus directory that is used for bots.
_CORPUS_FOR_BOTS_DIR = os.path.join(_THIS_DIR, os.path.pardir, os.path.pardir,
                                    'testing', 'libfuzzer',
                                    'fuzzer_corpus_for_bots')

# Dummy corpus in case real corpus doesn't exist.
_DUMMY_INPUT_CONTENTS = 'dummy input just to have at least one corpus unit'
_DUMMY_INPUT_FILENAME = 'dummy_corpus_input'

# Used for running fuzzer targets in code coverage config.
_DUMMY_CORPUS_DIRECTORY = 'dummy_corpus_dir_which_should_be_empty'

_LIBFUZZER_FLAGS = ['-merge=1', '-timeout=60', '-rss_limit_mb=8192']

_SLEEP_DURATION_SECONDS = 8


def _PrepareCorpus(fuzzer_name, output_dir):
  """Prepares the corpus to run fuzzer target.

  If a corpus for bots is available, use it directly, otherwise, creates a
  dummy corpus.

  Args:
    fuzzer_name (str): Name of the fuzzer to create corpus for.
    output_dir (str): An output directory to store artifacts.

  Returns:
    A path to the directory of the prepared corpus.
  """
  corpus_dir = os.path.join(output_dir, fuzzer_name + '_corpus')
  _RecreateDir(corpus_dir)

  corpus_for_bots = glob.glob(
      os.path.join(os.path.abspath(_CORPUS_FOR_BOTS_DIR), fuzzer_name, '*.zip'))
  if len(corpus_for_bots) >= 2:
    raise RuntimeError(
        'Expected only one, but multiple versions of corpus exit')

  if len(corpus_for_bots) == 1:
    zipfile.ZipFile(corpus_for_bots[0]).extractall(path=corpus_dir)
    return corpus_dir

  logging.info('Corpus for %s does not exist, create a dummy corpus input',
               fuzzer_name)
  dummy_input_path = os.path.join(corpus_dir, _DUMMY_INPUT_FILENAME)
  with open(dummy_input_path, 'wb') as fh:
    fh.write(_DUMMY_INPUT_CONTENTS)

  return corpus_dir


def _ParseCommandArguments():
  """Adds and parses relevant arguments for tool comands.

  Returns:
    A dictionary representing the arguments.
  """
  arg_parser = argparse.ArgumentParser()

  arg_parser.add_argument(
      '-f',
      '--fuzzer',
      type=str,
      required=True,
      help='Path to the fuzz target executable.')

  arg_parser.add_argument(
      '-o',
      '--output-dir',
      type=str,
      required=True,
      help='Output directory where corpus and coverage dumps can be stored in.')

  arg_parser.add_argument(
      '-t',
      '--timeout',
      type=int,
      required=True,
      help='Timeout value for running a single fuzz target.')

  # Ignored. Used to comply with isolated script contract, see chromium_tests
  # and swarming recipe modules for more details.
  arg_parser.add_argument(
      '--isolated-script-test-output',
      type=str,
      required=False,
      help=argparse.SUPPRESS)

  # Ditto.
  arg_parser.add_argument(
      '--isolated-script-test-perf-output',
      type=str,
      required=False,
      help=argparse.SUPPRESS)

  if len(sys.argv) == 1:
    arg_parser.print_help()
    sys.exit(1)

  args = arg_parser.parse_args()

  assert os.path.isfile(
      args.fuzzer), ("Fuzzer '%s' does not exist." % args.fuzzer)

  assert os.path.isdir(
      args.output_dir), ("Output dir '%s' does not exist." % args.output_dir)

  assert args.timeout > 0, 'Invalid timeout value: %d.' % args.timeout

  return args


def _RecreateDir(dir_path):
  if os.path.exists(dir_path):
    shutil.rmtree(dir_path)
  os.mkdir(dir_path)


def _RunFuzzTarget(fuzzer, fuzzer_name, output_dir, corpus_dir, timeout):
  # The way we run fuzz targets in code coverage config (-merge=1) requires an
  # empty directory to be provided to fuzz target. We run fuzz targets with
  # -merge=1 because that mode is crash-resistant.
  dummy_corpus_dir = os.path.join(output_dir, _DUMMY_CORPUS_DIRECTORY)
  _RecreateDir(dummy_corpus_dir)

  cmd = [fuzzer] + _LIBFUZZER_FLAGS + [dummy_corpus_dir, corpus_dir]

  try:
    _RunWithTimeout(cmd, timeout)
  except Exception as e:
    logging.info('Failed to run %s: %s', fuzzer_name, e)

  shutil.rmtree(dummy_corpus_dir)


def _RunWithTimeout(cmd, timeout):
  logging.info('Run fuzz target using the following command: %s', str(cmd))

  # TODO: we may need to use |creationflags=subprocess.CREATE_NEW_PROCESS_GROUP|
  # on Windows or send |signal.CTRL_C_EVENT| signal if the process times out.
  runner = subprocess.Popen(cmd)

  timer = 0
  while timer < timeout and runner.poll() is None:
    time.sleep(_SLEEP_DURATION_SECONDS)
    timer += _SLEEP_DURATION_SECONDS

  if runner.poll() is None:
    try:
      logging.info('Fuzz target timed out, interrupting it.')
      # libFuzzer may spawn some child processes, that is why we have to call
      # os.killpg, which would send the signal to our Python process as well, so
      # we just catch and ignore it in this try block.
      os.killpg(os.getpgid(runner.pid), signal.SIGINT)
    except KeyboardInterrupt:
      # Python's default signal handler raises KeyboardInterrupt exception for
      # SIGINT, suppress it here to prevent interrupting the script itself.
      pass

    runner.communicate()

  logging.info('Finished running the fuzz target.')


def Main():
  log_format = '[%(asctime)s %(levelname)s] %(message)s'
  logging.basicConfig(level=logging.INFO, format=log_format)

  args = _ParseCommandArguments()
  fuzzer_name = os.path.splitext(os.path.basename(args.fuzzer))[0]
  corpus_dir = _PrepareCorpus(fuzzer_name, args.output_dir)
  start_time = time.time()
  _RunFuzzTarget(args.fuzzer, fuzzer_name, args.output_dir, corpus_dir,
                 args.timeout)
  end_time = time.time()
  shutil.rmtree(corpus_dir)

  if args.isolated_script_test_output:
    # TODO(crbug.com/41431115): Actually comply with the isolated script contract
    # on src/testing/scripts/common.
    with open(args.isolated_script_test_output, 'w') as f:
      json.dump({
          'version': 3,
          'interrupted': False,
          'path_delimiter': '.',
          'seconds_since_epoch': int(start_time),
          'num_failures_by_type': {
              'FAIL': 0,
              'PASS': 1
          },
          'num_regressions': 0,
          'tests': {
              fuzzer_name: {
                  'expected': 'PASS',
                  'actual': 'PASS',
                  'times': [int(end_time - start_time),]
              },
          }
      }, f)

  return 0


if __name__ == '__main__':
  sys.exit(Main())
