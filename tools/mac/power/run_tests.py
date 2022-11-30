#!/usr/bin/env python3

# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys
import unittest

if __name__ == '__main__':
  logging.basicConfig(format='%(levelname)s: %(message)s',
                      level=logging.WARNING)

  suite = unittest.TestSuite()
  loader = unittest.TestLoader()
  suite.addTests(
      loader.discover(start_dir=os.path.dirname(__file__), pattern='*_test.py'))
  res = unittest.TextTestRunner(verbosity=2).run(suite)
  if res.wasSuccessful():
    sys.exit(0)
  else:
    sys.exit(1)
