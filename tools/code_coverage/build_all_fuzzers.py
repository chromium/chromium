#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Build all Chromium libfuzzer targets that have corresponding corpora.

  * Example usage: build_all_fuzzers.py --output-dir OUTPUT_DIR
"""
import argparse
import coverage_consts
import logging
import subprocess


def _ParseCommandArguments():
  """Adds and parses relevant arguments for tool comands.

  Returns:
    A dictionary representing the arguments.
  """
  arg_parser = argparse.ArgumentParser()
  arg_parser.usage = __doc__

  arg_parser.add_argument(
      '--output-dir',
      type=str,
      help=('Directory where fuzzers are to be built. GN args are assumed to '
            'have already been set.'))
  args = arg_parser.parse_args()
  return args


args = _ParseCommandArguments()


def try_build(total_fuzzer_target):
  subprocess_cmd = ['autoninja', '-C', args.output_dir]
  subprocess_cmd.extend(total_fuzzer_target)
  logging.info("Build command: %s" % subprocess_cmd)
  try:
    subprocess.check_call(subprocess_cmd)
  except:
    logging.error("An error occured while building the fuzzers.")
    exit


logging.info("Building all fuzzers")
total_fuzzer_target = []
for count, fuzzer_target in enumerate(coverage_consts.FUZZERS_WITH_CORPORA, 1):
  total_fuzzer_target.append(fuzzer_target)
  if count % 200 == 0:
    # Autoninja throws a "path has too many components" error if you try to
    # to build too many targets at once, so clear the buffer every 350 targets.
    try_build(total_fuzzer_target)
    total_fuzzer_target = []
if total_fuzzer_target:
  try_build(total_fuzzer_target)
logging.info("Built all fuzzers")
