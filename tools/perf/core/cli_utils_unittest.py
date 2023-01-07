# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import core.cli_utils


class CliUtilsTest(unittest.TestCase):
  def testMergeIndexRanges(self):
    ranges = [(0, 6), (7, 11), (4, 8), (8, 9)]
    merged = core.cli_utils.MergeIndexRanges(ranges)
    self.assertEqual([(0, 11)], merged)

  def testMergeIndexRangesEmpty(self):
    ranges = []
    merged = core.cli_utils.MergeIndexRanges(ranges)
    self.assertEqual([], merged)

  def testMergeIndexRangesNoMerge(self):
    ranges = [(7, 11), (0, 6)]
    merged = core.cli_utils.MergeIndexRanges(ranges)
    self.assertEqual([(0, 6), (7, 11)], merged)

  def testMergeIndexRangesEdgeCase(self):
    ranges = [(0, 8), (8, 11), (11, 12)]
    merged = core.cli_utils.MergeIndexRanges(ranges)
    self.assertEqual([(0, 12)], merged)

  def testMergeIndexRangesInvalidRange(self):
    with self.assertRaises(ValueError):
      ranges = [(0, 8), (8, 5)]
      core.cli_utils.MergeIndexRanges(ranges)
