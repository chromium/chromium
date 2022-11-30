#!/usr/bin/env python
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Create tarball of differences."""

import argparse
import json
import os
import shutil
import sys
import tarfile
import tempfile


def CreateArchive(first, second, input_files, output_file):
  """Create archive of input files to output_dir.

  Args:
    first: the first build directory.
    second: the second build directory.
    input_files: list of input files to be archived.
    output_file: an output file.
  """
  with tarfile.open(name=output_file, mode='w:gz') as tf:
    for f in input_files:
      tf.add(os.path.join(first, f))
      tf.add(os.path.join(second, f))


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('-f', '--first-build-dir',
                      help='The first build directory')
  parser.add_argument('-s', '--second-build-dir',
                      help='The second build directory')
  parser.add_argument('--json-input',
                      help='JSON file to specify list of files to archive.')
  parser.add_argument('--output', help='output filename.')
  args = parser.parse_args()

  if not args.first_build_dir:
    parser.error('--first-build-dir is required')
  if not args.second_build_dir:
    parser.error('--second-build-dir is required')
  if not args.json_input:
    parser.error('--json-input is required')
  if not args.output:
    parser.error('--output is required')

  with open(args.json_input) as f:
    input_files = json.load(f)

  CreateArchive(args.first_build_dir, args.second_build_dir, input_files,
                args.output)


if __name__ == '__main__':
  sys.exit(main())
