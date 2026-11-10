#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import pathlib
import sys
from typing import List

import setup_modules  # pylint: disable=unused-import

import typ
import chromium_src.tools.metrics.common.path_util as path_util
import chromium_src.tools.metrics.python_support.dependency_solver as dependency_solver
import chromium_src.tools.metrics.python_support.script_checker as script_checker
import chromium_src.tools.metrics.python_support.tests_helpers as tests_helpers


def _GetAffectedTestsPaths(affected_files: List[str]) -> List[str]:
  """Calculates paths of tests affected by the modified files."""
  deps_graph = dependency_solver.scan_directory_dependencies(
    path_util.METRICS_TOOLS_PATH,
    report_relative_to=path_util.CHROMIUM_SRC_PATH,
  )

  tests_to_run = tests_helpers.get_affected_tests(
    set(pathlib.Path(p) for p in affected_files), deps_graph
  )

  return [str(t.file_path) for t in tests_to_run]


def _RunTestFiles(tests_files_paths: List[str], extra_args: List[str]) -> int:
  """Runs the specified test files using typ."""
  # Given no tests, typ would run every test it can find.
  if not tests_files_paths:
    print('No tests to run.')
    return 0

  print(f'Running {len(tests_files_paths)} test files...')

  # typ reports each test to ResultDB itself when LUCI_CONTEXT is set (as on
  # the bots); testing/scripts/metrics_python_tests.py relies on that.
  return typ.main(  # type: ignore[attr-defined]
    argv=extra_args,
    tests=[os.path.relpath(p) for p in tests_files_paths],
    top_level_dirs=tests_helpers.TEST_DIRECTORIES_RELATIVE_TO_SRC,
    jobs=128,
  )


def main():
  parser = argparse.ArgumentParser(description='Run metrics python tests.')
  parser.add_argument(
    '--affected_only',
    action='store_true',
    help='Only run the tests affected by the files listed on stdin, one per '
    'line.',
  )
  args, typ_args = parser.parse_known_args()

  if args.affected_only:
    tests_files_paths = _GetAffectedTestsPaths(
      script_checker.read_stdin_lines()
    )
  else:
    tests_files_paths = [str(p) for p in tests_helpers.find_all_tests()]

  return _RunTestFiles(tests_files_paths, typ_args)


if __name__ == '__main__':
  sys.exit(main())
