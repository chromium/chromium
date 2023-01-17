#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for the 'grit newgrd' tool.'''


import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

import unittest

from grit import util
from grit.tool import newgrd


class DummyOpts:
  """Options needed by NewGrd."""


class NewgrdUnittest(unittest.TestCase):

  def testNewFile(self):
    """Create a new file."""
    tool = newgrd.NewGrd()
    with util.TempDir({}) as output_dir:
      output_file = os.path.join(output_dir.GetPath(), 'new.grd')
      self.assertIsNone(tool.Run(DummyOpts(), [output_file]))
      self.assertTrue(os.path.exists(output_file))

  def testMissingFile(self):
    """Verify failure w/out file output."""
    tool = newgrd.NewGrd()
    ret = tool.Run(DummyOpts(), [])
    self.assertIsNotNone(ret)
    self.assertGreater(ret, 0)

  def testTooManyArgs(self):
    """Verify failure w/too many outputs."""
    tool = newgrd.NewGrd()
    ret = tool.Run(DummyOpts(), ['a', 'b'])
    self.assertIsNotNone(ret)
    self.assertGreater(ret, 0)


if __name__ == '__main__':
  unittest.main()
