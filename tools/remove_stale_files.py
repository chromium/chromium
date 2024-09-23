#!/usr/bin/env python
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys


def RemoveAllStaleFiles(paths):
  """Check if any stale files (e.g. old GCS archives) are on filesystem, and
  remove them."""
  for path in paths:
    try:
      if os.path.exists(path) and not os.path.isdir(path):
        os.remove(path)
    except OSError:
      # Wrap OS calls in try/except in case another process touched this file.
      pass


if __name__ == '__main__':
  RemoveAllStaleFiles(sys.argv[1:])
