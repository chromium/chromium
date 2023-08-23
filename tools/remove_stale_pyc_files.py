#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys


def RemoveAllStalePycFiles(base_dir):
  """Scan directories for old .pyc files without a .py file and delete them."""
  for dirname, _, filenames in os.walk(base_dir, topdown=False):
    if '.svn' in dirname or '.git' in dirname:
      continue
    for filename in filenames:
      root, ext = os.path.splitext(filename)
      if ext != '.pyc':
        continue

      pyc_path = os.path.join(dirname, filename)
      py_path = os.path.join(dirname, root + '.py')

      try:
        if not os.path.exists(py_path):
          os.remove(pyc_path)
      except OSError:
        # Wrap OS calls in try/except in case another process touched this file.
        pass


if __name__ == '__main__':
  for path in sys.argv[1:]:
    RemoveAllStalePycFiles(path)
