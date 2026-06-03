# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper script to validate GN file references."""

import os
import sys

import setup_modules  # pylint: disable=unused-import
import chromium_src.tools.metrics.python_support.tests_helpers as tests_helpers

if __name__ == '__main__':
  missing_files = tests_helpers.validate_gn_sources('metrics_python_tests')
  if missing_files:
    print('There are test files that are not listed in tools/metrics/BUILD.gn')
    for f in sorted(missing_files):
      print(f'   \'//{f}\',')
    sys.exit(1)
  sys.exit(0)
