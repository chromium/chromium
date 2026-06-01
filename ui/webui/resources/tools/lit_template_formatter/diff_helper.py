#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
  A thin wrapper around Python's difflib.unified_diff() that
  prints the unified diff between 2 files to stdout.
"""

import argparse
import difflib
import sys


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('file1')
  parser.add_argument('file2')
  args = parser.parse_args()

  file1 = args.file1
  file2 = args.file2

  try:
    with open(file1, 'r', encoding='utf-8') as f1:
      lines1 = f1.readlines()
    with open(file2, 'r', encoding='utf-8') as f2:
      lines2 = f2.readlines()
  except Exception as e:
    print(f"Error reading files: {e}", file=sys.stderr)
    sys.exit(2)

  # Use unified_diff to generate diff similar to 'git diff'
  diff = difflib.unified_diff(
      lines1, lines2, fromfile=file1, tofile=file2, lineterm='')

  for line in diff:
    print(line)

  sys.exit(0)


if __name__ == '__main__':
  main()
