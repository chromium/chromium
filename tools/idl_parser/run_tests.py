#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import glob
import os
import sys
import unittest

if __name__ == '__main__':
  suite = unittest.TestSuite()
  cur_dir = os.path.dirname(os.path.realpath(__file__))
  for testname in glob.glob(os.path.join(cur_dir, '*_test.py')):
    print('Adding Test: ' + testname)
    module = __import__(os.path.basename(testname)[:-3])
    suite.addTests(unittest.defaultTestLoader.loadTestsFromModule(module))
  result = unittest.TextTestRunner(verbosity=2).run(suite)
  if result.wasSuccessful():
    sys.exit(0)
  else:
    sys.exit(1)
