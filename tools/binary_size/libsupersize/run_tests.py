#!/usr/bin/env vpython3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys
import unittest

if __name__ == '__main__':
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.WARNING,
      format='%(levelname)5s %(filename)15s(%(lineno)3d): %(message)s')

  suite = unittest.TestSuite()
  loader = unittest.TestLoader()
  suite.addTests(
      loader.discover(start_dir=os.path.dirname(__file__), pattern='*_test.py'))
  res = unittest.TextTestRunner(verbosity=2).run(suite)
  if res.wasSuccessful():
    sys.exit(0)
  else:
    sys.exit(1)
