#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

import setup_modules

import typ
import chromium_src.tools.metrics.python_support.tests_helpers as tests_helpers

_EXCLUDE_TEST_FILES_PATHS = [os.path.abspath(__file__)]

def main():
  tests_files_paths = list(tests_helpers.find_all_tests())
  filtered_tests_files_paths = [
      p for p in tests_files_paths if p not in _EXCLUDE_TEST_FILES_PATHS
  ]

  print(f"Running {len(filtered_tests_files_paths)} test files...")

  return typ.main(tests=filtered_tests_files_paths,
                  top_level_dirs=tests_helpers.TEST_DIRECTORIES_RELATIVE_TO_SRC,
                  jobs=128)


if __name__ == '__main__':
  sys.exit(main())
