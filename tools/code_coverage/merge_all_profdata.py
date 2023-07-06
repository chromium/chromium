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

  arg_parser.add_argument('--llvm-profdata',
                          required=True,
                          type=str,
                          help='Location of LLVM profdata tool')
  arg_parser.add_argument('--profdata-dir',
                          required=True,
                          type=str,
                          help='Directory in which profdata files are stored.')
  arg_parser.add_argument(
      '--outfile',
      type=str,
      required=True,
      help=('Directory where the coverage report should go. '
            'Default is out/report.'))

  args = arg_parser.parse_args()

  return args


args = _ParseCommandArguments()
targets_to_cover = []
for profdata_file in os.listdir(args.profdata_dir):
  targets_to_cover.append(os.path.join(args.profdata_dir, profdata_file))

subprocess_cmd = [args.llvm_profdata, 'merge', '-o', args.outfile]

for target in targets_to_cover:
  subprocess_cmd.append(target)
try:
  subprocess.check_call(subprocess_cmd)
except:
  logging.error("An error occured while merging the profdata.")
  exit(1)
