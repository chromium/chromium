#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit test for lazy_re.
'''

from __future__ import print_function

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

import re
import unittest

from grit import lazy_re


class LazyReUnittest(unittest.TestCase):

  def testCreatedOnlyOnDemand(self):
    rex = lazy_re.compile('bingo')
    self.assertEqual(None, rex._lazy_re)
    self.assertTrue(rex.match('bingo'))
    self.assertNotEqual(None, rex._lazy_re)

  def testJustKwargsWork(self):
    rex = lazy_re.compile(flags=re.I, pattern='BiNgO')
    self.assertTrue(rex.match('bingo'))

  def testPositionalAndKwargsWork(self):
    rex = lazy_re.compile('BiNgO', flags=re.I)
    self.assertTrue(rex.match('bingo'))


if __name__ == '__main__':
  unittest.main()
