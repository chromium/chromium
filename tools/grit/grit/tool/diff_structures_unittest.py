#!/usr/bin/env python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for the 'grit newgrd' tool.'''

from __future__ import print_function

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

import unittest

from grit.tool import diff_structures


class DummyOpts(object):
  """Options needed by NewGrd."""


class DiffStructuresUnittest(unittest.TestCase):

  def testMissingFiles(self):
    """Verify failure w/out file inputs."""
    tool = diff_structures.DiffStructures()
    ret = tool.Run(DummyOpts(), [])
    self.assertIsNotNone(ret)
    self.assertGreater(ret, 0)

    ret = tool.Run(DummyOpts(), ['left'])
    self.assertIsNotNone(ret)
    self.assertGreater(ret, 0)

  def testTooManyArgs(self):
    """Verify failure w/too many inputs."""
    tool = diff_structures.DiffStructures()
    ret = tool.Run(DummyOpts(), ['a', 'b', 'c'])
    self.assertIsNotNone(ret)
    self.assertGreater(ret, 0)


if __name__ == '__main__':
  unittest.main()
