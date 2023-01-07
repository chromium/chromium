#!/usr/bin/env vpython3

# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import re
from pathlib import Path

import util

# Path to the directory where this script is.
SCRIPT_DIR = Path(__file__).resolve().parent

# Absolute path to chrome/src.
SRC_DIR = SCRIPT_DIR.parents[3]

# Default value of the |annotations_file| argument.
ANNOTATIONS_XML_PATH = 'tools/traffic_annotation/summary/annotations.xml'

if __name__ == '__main__':
  args_parser = argparse.ArgumentParser(
      description='Reads annotations.xml and outputs a mapping of unique IDs '
      'to their hashes.',
      prog='hashes.py')
  args_parser.add_argument(
      'annotations_file',
      nargs='?',
      default=SRC_DIR / ANNOTATIONS_XML_PATH,
      type=Path,
      help='Optional path to a summary file containing all annotations. '
      'Defaults to {}.'.format(ANNOTATIONS_XML_PATH))

  args = args_parser.parse_args()

  xml = args.annotations_file.read_text(encoding='utf-8')
  unique_ids = sorted(re.findall(r'id="([^"]+)"', xml))
  for unique_id in unique_ids:
    print('{}\t{}'.format(unique_id, util.compute_hash_value(unique_id)))
