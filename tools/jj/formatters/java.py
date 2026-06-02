#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys


def main():
  # jj fix will execute:
  # tools/jj/formatters/java.py $path [line-range-arg...]
  # where line-range-arg is e.g. --lines=5:10 --lines=20:25.

  script_dir = os.path.dirname(os.path.abspath(__file__))
  tool = os.path.abspath(
      os.path.join(
          script_dir,
          "..",
          "..",
          "..",
          "third_party",
          "google-java-format",
          "google-java-format",
      ))
  if not os.path.exists(tool):
    # If the tool doesn't exist, just pass stdin directly to stdout unchanged.
    sys.stdout.buffer.write(sys.stdin.buffer.read())
    return 0

  # Collect arguments: we want to pass any --lines=... arguments.
  line_args = [arg for arg in sys.argv[1:] if arg.startswith('--lines=')]

  base_cmd = [tool, '--aosp']

  # First pass: Format either the full file or the specific line ranges.
  # Input is read from stdin, output is captured.
  input_data = sys.stdin.buffer.read()

  # Format step
  format_cmd = base_cmd + line_args + ['-']
  proc1 = subprocess.run(format_cmd,
                         input=input_data,
                         stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE)

  if proc1.returncode != 0:
    # If formatting fails, print stderr and exit with code.
    sys.stderr.buffer.write(proc1.stderr)
    sys.exit(proc1.returncode)

  formatted_data = proc1.stdout

  # Second pass: If we formatted specific lines, also run a separate pass to
  # fix imports. Formatting the full file automatically cleans up imports, but
  # formatting specific line ranges restricts optimizations to only those lines,
  # so imports outside the line ranges would otherwise be ignored.
  if line_args:
    imports_cmd = base_cmd + ['--fix-imports-only', '-']
    proc2 = subprocess.run(imports_cmd,
                           input=formatted_data,
                           stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE)
    if proc2.returncode != 0:
      sys.stderr.buffer.write(proc2.stderr)
      sys.exit(proc2.returncode)
    formatted_data = proc2.stdout

  sys.stdout.buffer.write(formatted_data)
  return 0


if __name__ == '__main__':
  sys.exit(main())
