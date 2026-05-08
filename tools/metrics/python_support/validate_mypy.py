# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper script allowing to run a MyPy analysis as a standalone script."""

import os
import sys

import setup_modules  # pylint: disable=unused-import
import chromium_src.tools.metrics.python_support.mypy_helpers as mypy_helpers
from chromium_src.tools.metrics.common.path_util import METRICS_TOOLS_PATH

if __name__ == '__main__':
  mypy_errors = mypy_helpers.run_mypy_and_filter_irrelevant(
      str(METRICS_TOOLS_PATH))
  print("\n".join(f" * {error}" for error in mypy_errors))

  sys.exit(len(mypy_errors))
