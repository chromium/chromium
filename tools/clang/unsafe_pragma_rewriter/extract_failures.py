#!/usr/bin/env python
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import re


def extract_failed_files(log_content):
  """Scans a compile log and returns the set of filenames that failed
  to compile."""
  failed_files = set()
  # Regex to match lines like '../../ipc/ipc_logging.cc:265:5: error:'
  # It captures the path and filename.
  error_pattern = re.compile(
      rb"^(?P<filepath>[^:]+):\d+:\d+:.*\s*(error|warning):")

  for line in log_content.splitlines():
    match = error_pattern.match(line)
    if match:
      filepath = match.group('filepath').decode('ascii')
      failed_files.add(filepath)
  return failed_files


def main():
  """Reads the compile log from standard input, extracts failed files,
  and prints them one per line to standard output."""
  log_content = sys.stdin.buffer.read()
  failed_files = extract_failed_files(log_content)
  for filename in sorted(list(failed_files)):
    print(filename.removeprefix("../../"))


if __name__ == "__main__":
  main()
