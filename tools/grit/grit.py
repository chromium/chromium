#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Bootstrapping for GRIT.
'''


import os
import sys

import grit.grit_runner


if __name__ == '__main__':
  ret = grit.grit_runner.Main(sys.argv[1:])
  # Use os._exit() instead of sys.exit() to skip the Python garbage
  # collector teardown at script exit, which takes 1.4s for the 100MB AST.
  # As os._exit() stops the process immediately without calling cleanup handlers,
  # we must explicitly flush stdout and stderr to avoid losing any output.
  # Note: Since os._exit() bypasses Python's normal file cleanup, we must ensure
  # that all other file objects are properly closed inside grit_runner.Main().
  # See https://docs.python.org/3.14/library/os.html#os._exit
  sys.stdout.flush()
  sys.stderr.flush()
  os._exit(ret or 0)
