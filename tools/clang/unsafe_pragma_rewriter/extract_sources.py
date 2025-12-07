#!/usr/bin/env python
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import re


def extract_tried_files(log_content):
  """Scans a compile log and returns a set of filenames that tried to
  be compiled."""
  tried_files = set()
  # Regex to match lines like:
  # '[1688/67026] 11.01s ... clang++ ... -c ./../ipc/ipc_logging.cc'
  # It captures the path and filename.
  pattern = re.compile(rb"^\[\d+/\d+\] .*clang\+\+ .* -c ([^ ]+) -o [^ ]+")

  for line in log_content.splitlines():
    match = pattern.match(line)
    if match:
      filepath = match.group(1).decode('ascii')
      tried_files.add(filepath)
  return tried_files


def main():
  """Reads the compile log from standard input, extracts tried files,
  and prints them one per line to standard output."""
  log_content = sys.stdin.buffer.read()
  tried_files = extract_tried_files(log_content)
  for filename in sorted(list(tried_files)):
    print(filename.removeprefix("../../"))


if __name__ == "__main__":
  main()
