#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
from typing import Iterable

import setup_modules
import typ

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
_CHROMIUM_SRC_DIR = os.path.dirname(os.path.dirname(_THIS_DIR))

# Paths relative to src/ that we want to run the test from
_TEST_DIRECTORIES_TO_SCAN = [
    'tools/metrics', 'tools/variations', 'tools/json_comment_eater',
    'tools/json_to_struct'
]

_EXPECTED_TEST_FILES_SUFFIXES = ['test.py', 'tests.py']
_EXCLUDE_TEST_FILES_PATHS = [os.path.abspath(__file__)]

_TEST_DIRECTORIES_RELATIVE_TO_SRC = [
    os.path.join(_CHROMIUM_SRC_DIR, d) for d in _TEST_DIRECTORIES_TO_SCAN
]


def _is_test_file(file_path: str) -> bool:
  return any(
      file_path.endswith(suffix) for suffix in _EXPECTED_TEST_FILES_SUFFIXES)


def _find_all_tests_in(directory_path: str) -> Iterable[str]:
  for dir, _, files in os.walk(directory_path):
    for file in files:
      if _is_test_file(file):
        yield os.path.join(dir, file)


def _find_all_tests() -> Iterable[str]:
  """Finds all python tests in all directories listed in DIRECTORIES_TO_SCAN"""
  all_test_files = []
  for dir in _TEST_DIRECTORIES_RELATIVE_TO_SRC:
    all_test_files.extend(_find_all_tests_in(dir))
  return all_test_files


def main():
  tests_files_paths = list(set(_find_all_tests()))
  filtered_tests_files_paths = [
      p for p in tests_files_paths if p not in _EXCLUDE_TEST_FILES_PATHS
  ]
  print(f"Running {len(filtered_tests_files_paths)} test files...")

  typ.main(tests=filtered_tests_files_paths,
           top_level_dirs=_TEST_DIRECTORIES_RELATIVE_TO_SRC,
           jobs=1)


if __name__ == '__main__':
  sys.exit(main())
