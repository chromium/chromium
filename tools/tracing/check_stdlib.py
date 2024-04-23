#!/usr/bin/env vpython3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A wrapper script for //third_party/perfetto/tools/check_sql_modules.py
# to check the Chrome Stdlib.

import subprocess
import os
import sys


def main():
  perfetto_dir = os.path.abspath(
      os.path.join(__file__, "..", "..", "..", "third_party", "perfetto"))
  tool = os.path.join(perfetto_dir, "tools", "check_sql_modules.py")
  stdlib_sources = os.path.join(perfetto_dir, "..", "..", "base", "tracing",
                                "stdlib")
  completed_process = subprocess.run(
      ["vpython3", tool, "--stdlib-sources", stdlib_sources],
      check=False,
      capture_output=True)
  sys.stderr.buffer.write(completed_process.stderr)
  sys.stdout.buffer.write(completed_process.stdout)
  return completed_process.returncode


if __name__ == '__main__':
  sys.exit(main())
