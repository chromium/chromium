# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Utility functions for resolving file paths in histograms scripts.'''

import pathlib

CHROMIUM_SRC_PATH = pathlib.Path(__file__).resolve().parents[3]
METRICS_TOOLS_PATH = pathlib.Path(__file__).resolve().parents[1]


def GetInputFile(src_relative_file_path: str) -> str:
  return str(GetInputFilePath(src_relative_file_path))


def GetInputFilePath(src_relative_file_path: str) -> pathlib.Path:
  return (CHROMIUM_SRC_PATH / src_relative_file_path).resolve()
