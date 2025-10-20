# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper functions for tests in //tools/licenses."""
import os

_OS_ROOT = os.path.abspath(os.sep)


def path_from_root(*paths: str):
  return os.path.join(_OS_ROOT, *paths)
