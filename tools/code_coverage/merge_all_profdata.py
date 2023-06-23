#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" Merge all the profdata files in PROFDATA_DIR, utilizing the binaries in
    BINARIES_DIR, to create a unified coverage report in REPORT_DIR. (If no
    REPORT_DIR is provided, defaults to `out/report`.)

  * Example usage: merge_all_profdata.py --profdata-dir [PROFDATA_DIR]
    --binaries-dir [BINARIES_DIR] [--report-dir [REPORT_DIR]]
"""

import argparse
import logging
import os
import subprocess


def _ParseCommandArguments():
  """Adds and parses relevant arguments for tool commands.

  Returns:
    A dictionary representing the arguments.
  """
  arg_parser = argparse.ArgumentParser()
  arg_parser.usage = __doc__

  arg_parser.add_argument('--profdata-dir',
                          required=True,
                          type=str,
                          help='Directory in which profdata files are stored.')
  arg_parser.add_argument('--binaries-dir',
                          required=True,
                          type=str,
                          help=('Directory where binaries have been built.'))
  arg_parser.add_argument(
      '--report-dir',
      type=str,
      const='out/report',
      default='out/report',
      nargs='?',
      help=('Directory where the coverage report should go. '
            'Default is out/report.'))

  args = arg_parser.parse_args()

  return args


args = _ParseCommandArguments()
targets_to_cover = []
for profdata_file in os.listdir(args.profdata_dir):
  target_name = profdata_file.split(".")[0]
  target_path = os.path.join(args.binaries_dir, target_name)
  profdata_file = target_name + ".profdata"
  profdata_path = os.path.join(args.profdata_dir, profdata_file)
  if os.path.isfile(target_path) and os.path.isfile(profdata_path):
    targets_to_cover.append((target_name, profdata_path))

subprocess_cmd = ['python3', 'tools/code_coverage/coverage.py']
for target in targets_to_cover:
  subprocess_cmd.append(target[0])
subprocess_cmd.extend(['-b', args.binaries_dir, '-o', args.report_dir])
for target in targets_to_cover:
  subprocess_cmd.extend(['-p', target[1]])
try:
  subprocess.check_call(subprocess_cmd)
except:
  logging.error("An error occured while merging the profdata.")
  exit(1)
