#!/usr/bin/env python3
# Copyright (c) 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for json_values_converter.py.

It tests json_values_converter.py.
"""

import argparse
import os
import sys


def CompareFiles(file1, file2):
  return open(file1, 'r').read() == open(file2, 'r').read()


def TouchStamp(stamp_path):
  dir_name = os.path.dirname(stamp_path)
  if not os.path.isdir(dir_name):
    os.makedirs(dir_name)

  with open(stamp_path, 'a'):
    os.utime(stamp_path, None)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--stamp',
                      help='Path to touch on success.')
  parser.add_argument('files', nargs='+',
                      help='Files to compare.')

  args = parser.parse_args()

  passed = True
  for i, j in zip(args.files[::2], args.files[1::2]):
    passed = passed and CompareFiles(i, j)

  if passed and args.stamp:
    TouchStamp(args.stamp)

  return not passed

if __name__ == '__main__':
  sys.exit(main())
