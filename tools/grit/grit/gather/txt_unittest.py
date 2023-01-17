#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for TxtFile gatherer'''

import os
import sys
import unittest
import io

if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

from grit.gather import txt


class TxtUnittest(unittest.TestCase):
  def testGather(self):
    input = io.StringIO('Hello there\nHow are you?')
    gatherer = txt.TxtFile(input)
    gatherer.Parse()
    self.assertTrue(gatherer.GetText() == input.getvalue())
    self.assertTrue(len(gatherer.GetCliques()) == 1)
    self.assertTrue(gatherer.GetCliques()[0].GetMessage().GetRealContent() ==
                    input.getvalue())


if __name__ == '__main__':
  unittest.main()
