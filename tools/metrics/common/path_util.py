# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions for resolving file paths in histograms scripts."""

import os.path
from pathlib import Path

CHROMIUM_SRC_PATH = Path(__file__).resolve().parents[3]
METRICS_TOOLS_PATH = Path(__file__).resolve().parents[1]


def GetInputFile(src_relative_file_path: str) -> str:
  return str((CHROMIUM_SRC_PATH / src_relative_file_path).resolve())
