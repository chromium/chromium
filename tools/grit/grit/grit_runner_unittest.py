#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.py'''

import io
import os
import sys
import unittest

if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

from grit import util
import grit.grit_runner

class OptionArgsUnittest(unittest.TestCase):
  def setUp(self):
    self.buf = io.StringIO()
    self.old_stdout = sys.stdout
    sys.stdout = self.buf

  def tearDown(self):
    sys.stdout = self.old_stdout

  def testSimple(self):
    grit.grit_runner.Main(['-i',
                           util.PathFromRoot('grit/testdata/simple-input.xml'),
                           'test', 'bla', 'voff', 'ga'])
    output = self.buf.getvalue()
    self.assertTrue(output.count("'test'") == 0)  # tool name doesn't occur
    self.assertTrue(output.count('bla'))
    self.assertTrue(output.count('simple-input.xml'))


if __name__ == '__main__':
  unittest.main()
