#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

import setup_modules  # pylint: disable=unused-import


import typ
import chromium_src.tools.metrics.python_support.tests_helpers as tests_helpers


def main():
  tests_files_paths = [
    os.path.relpath(p) for p in tests_helpers.find_all_tests()
  ]

  print(f'Running {len(tests_files_paths)} test files...')

  # typ reports each test to ResultDB itself when LUCI_CONTEXT is set (as on
  # the bots); testing/scripts/metrics_python_tests.py relies on that.
  return typ.main(
    tests=tests_files_paths,
    top_level_dirs=tests_helpers.TEST_DIRECTORIES_RELATIVE_TO_SRC,
    jobs=128,
  )


if __name__ == '__main__':
  sys.exit(main())
