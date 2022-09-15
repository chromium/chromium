#!/usr/bin/env vpython3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

sys.path.append(os.path.abspath(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir)))
from core import path_util

path_util.AddTelemetryToPath()

from contrib.orderfile import orderfile


class Orderfile(unittest.TestCase):
  def setUp(self):
    # Increase failed test output to make updating easier.
    self.maxDiff = None

  def testDefaults(self):
    training = {
        s.NAME
        for s in orderfile.OrderfileStorySet(
            orderfile.OrderfileStorySet.TRAINING).RunSetStories()
    }
    self.assertEqual(orderfile.OrderfileStorySet.DEFAULT_TRAINING,
                     len(training))
    testing = {
        s.NAME
        for s in orderfile.OrderfileStorySet(
            orderfile.OrderfileStorySet.TESTING).RunSetStories()
    }
    self.assertEqual(orderfile.OrderfileStorySet.DEFAULT_TESTING, len(testing))
    self.assertEqual(0, len(testing & training))

  def test25TrainingStories(self):
    training = {
        s.NAME
        for s in
        orderfile.OrderfileStorySet(orderfile.OrderfileStorySet.TRAINING,
                                    num_training=25).RunSetStories()
    }
    self.assertEqual(25, len(training))
    testing = {
        s.NAME
        for s in
        orderfile.OrderfileStorySet(orderfile.OrderfileStorySet.TESTING,
                                    num_training=25).RunSetStories()
    }
    self.assertEqual(orderfile.OrderfileStorySet.DEFAULT_TESTING, len(testing))
    self.assertEqual(0, len(testing & training))

  def testTestingVariationStories(self):
    training = {
        s.NAME
        for s in orderfile.OrderfileStorySet(
            orderfile.OrderfileStorySet.TRAINING,
            num_training=25,
            num_variations=orderfile.OrderfileStorySet.NUM_VARIATION_BENCHMARKS,
            test_variation=0).RunSetStories()
    }
    testing = [{
        s.NAME
        for s in orderfile.OrderfileStorySet(
            orderfile.OrderfileStorySet.TESTING,
            num_training=25,
            num_variations=orderfile.OrderfileStorySet.NUM_VARIATION_BENCHMARKS,
            test_variation=i).RunSetStories()
    } for i in range(orderfile.OrderfileStorySet.NUM_VARIATION_BENCHMARKS)]
    self.assertEqual(25, len(training))
    for i in range(orderfile.OrderfileStorySet.NUM_VARIATION_BENCHMARKS):
      self.assertEqual(orderfile.OrderfileStorySet.DEFAULT_TESTING,
                       len(testing[i]))
      self.assertEqual(0, len(testing[i] & training))
      for j in range(i + 1,
                     orderfile.OrderfileStorySet.NUM_VARIATION_BENCHMARKS):
        self.assertEqual(0, len(testing[i] & testing[j]))


if __name__ == '__main__':
  unittest.main()
