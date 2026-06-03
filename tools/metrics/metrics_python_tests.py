#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import os
import sys

import setup_modules  # pylint: disable=unused-import

import argparse
import pathlib
from typing import List
import typ
import chromium_src.tools.metrics.common.path_util as path_util
import chromium_src.tools.metrics.python_support.dependency_solver as dependency_solver
import chromium_src.tools.metrics.python_support.tests_helpers as tests_helpers

_EXCLUDE_TEST_FILES_PATHS = [os.path.abspath(__file__)]


@contextlib.contextmanager
def _disable_luci_context():
  """Temporarily hides LUCI_CONTEXT to prevent typ from using ResultSink."""
  saved_context = os.environ.pop('LUCI_CONTEXT', None)
  try:
    yield
  finally:
    if saved_context is not None:
      os.environ['LUCI_CONTEXT'] = saved_context


def _GetAffectedTestsPaths(affected_files: List[str]) -> List[str]:
  """Calculates paths of tests affected by the modified files."""
  deps_graph = dependency_solver.scan_directory_dependencies(
      str(path_util.METRICS_TOOLS_PATH),
      report_relative_to=str(path_util.CHROMIUM_SRC_PATH))

  tests_to_run = tests_helpers.get_affected_tests(
      set(pathlib.Path(p) for p in affected_files), deps_graph)

  return [str(t.file_path) for t in tests_to_run]


def _FilterRelevantTestsPaths(tests_files_paths: List[str]) -> List[str]:
  """Filters out excluded test files and normalizes paths to be relative."""
  return [
      os.path.relpath(p) for p in tests_files_paths
      if os.path.abspath(p) not in _EXCLUDE_TEST_FILES_PATHS
  ]


def _RunTestFiles(filtered_paths: List[str], extra_args: List[str]) -> int:
  """Runs the specified test files using typ."""
  if not filtered_paths:
    print('No tests to run.')
    return 0

  print(f'Running {len(filtered_paths)} test files...')

  # Hide LUCI_CONTEXT from typ so it doesn't try to upload granular test
  # results to ResultSink. typ's hierarchical test IDs violate the presubmit
  # ResultDB schema, causing HTTP 400s and crashing the test run.
  # TODO(crbug.com/488365101): Reconsider usage of typ as a runner.
  with _disable_luci_context():
    return typ.main(  # type: ignore[attr-defined]
        argv=extra_args,
        tests=filtered_paths,
        top_level_dirs=tests_helpers.TEST_DIRECTORIES_RELATIVE_TO_SRC,
        jobs=128)


def main():
  parser = argparse.ArgumentParser(description='Run metrics python tests.')
  parser.add_argument(
      '--affected_only',
      nargs='*',
      help='Only run tests affected by the specified modified files.')
  args, unknown = parser.parse_known_args()

  if args.affected_only is not None:
    tests_files_paths = _GetAffectedTestsPaths(args.affected_only)
  else:
    tests_files_paths = list(tests_helpers.find_all_tests())

  filtered_paths = _FilterRelevantTestsPaths(tests_files_paths)
  return _RunTestFiles(filtered_paths, unknown)


if __name__ == '__main__':
  sys.exit(main())
