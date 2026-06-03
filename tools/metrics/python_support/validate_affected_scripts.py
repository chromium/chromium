# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper script to validate affected scripts."""

import os
import pathlib
import sys

import setup_modules  # pylint: disable=unused-import
import chromium_src.tools.metrics.common.path_util as path_util
import chromium_src.tools.metrics.python_support.dependency_solver as dependency_solver
import chromium_src.tools.metrics.python_support.script_checker as script_checker
import chromium_src.tools.metrics.python_support.tests_helpers as tests_helpers

if __name__ == '__main__':
  affected_files = sys.argv[1:]
  if not affected_files:
    sys.exit(0)

  src_path = path_util.CHROMIUM_SRC_PATH
  metrics_dir = path_util.METRICS_TOOLS_PATH

  deps_graph = dependency_solver.scan_directory_dependencies(
      str(metrics_dir), report_relative_to=str(src_path))

  scripts_to_test = tests_helpers.get_affected_testable_scripts(
      set(pathlib.Path(p) for p in affected_files), deps_graph)

  if not scripts_to_test:
    sys.exit(0)

  commands_failed = script_checker.check_scripts(scripts_to_test,
                                                 cwd=str(src_path))

  for res in commands_failed:
    print(res.error_message())

  sys.exit(len(commands_failed))
