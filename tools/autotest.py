#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import subprocess


def main():
  tools_dir = os.path.dirname(os.path.abspath(__file__))

  autotest_dir = os.path.join(tools_dir, "autotest")
  main_py = os.path.join(autotest_dir, "main.py")

  # Invoke the actual autotest tool as a separate process. This way
  # vpython can use the custom vpython spec instead of using that from
  # src.
  exe = "vpython3.bat" if sys.platform == "win32" else "vpython3"
  cmd = [exe, main_py] + sys.argv[1:]

  if sys.platform == "win32":
    result = subprocess.run(cmd)
    return result.returncode
  os.execvp("vpython3", cmd)


if __name__ == "__main__":
  sys.exit(main())
