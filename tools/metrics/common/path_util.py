# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions for resolving file paths in histograms scripts."""

import os.path


def GetInputFile(src_relative_file_path):
  """Converts a src/-relative file path into a path that can be opened."""
  depth = [os.path.dirname(__file__), '..', '..', '..']
  path = os.path.join(*(depth + src_relative_file_path.split('/')))
  return os.path.abspath(path)
