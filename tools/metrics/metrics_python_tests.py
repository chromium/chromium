#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import os
import sys

import setup_modules

import typ
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

def main():
  tests_files_paths = list(tests_helpers.find_all_tests())

  filtered_tests_files_paths = [
      os.path.relpath(p) for p in tests_files_paths
      if p not in _EXCLUDE_TEST_FILES_PATHS
  ]

  print(f"Running {len(filtered_tests_files_paths)} test files...")

  # Hide LUCI_CONTEXT from typ so it doesn't try to upload granular test
  # results to ResultSink. typ's hierarchical test IDs violate the presubmit
  # ResultDB schema, causing HTTP 400s and crashing the test run.
  # TODO(crbug.com/488365101): Reconsider usage of typ as a runner.
  with _disable_luci_context():
    return typ.main(
        tests=filtered_tests_files_paths,
        top_level_dirs=tests_helpers.TEST_DIRECTORIES_RELATIVE_TO_SRC,
        jobs=128)


if __name__ == '__main__':
  sys.exit(main())
