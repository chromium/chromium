#!/usr/bin/env vpython3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile
import unittest

import generate_orderfile_full


class TestOrderfileGenerator(unittest.TestCase):

  def testStepRecorder(self):
    """Checks that the step recorder records step timings correctly."""
    step_recorder = generate_orderfile_full.StepRecorder()
    self.assertFalse(step_recorder.ErrorRecorded())
    step_recorder.BeginStep('foo')
    self.assertFalse(step_recorder.ErrorRecorded())
    step_recorder.BeginStep('bar')
    self.assertFalse(step_recorder.ErrorRecorded())
    step_recorder.FailStep()
    self.assertEqual(2, len(step_recorder.timings))
    self.assertEqual('foo', step_recorder.timings[0][0])
    self.assertEqual('bar', step_recorder.timings[1][0])
    self.assertLess(0, step_recorder.timings[0][1])
    self.assertLess(0, step_recorder.timings[1][1])
    self.assertTrue(step_recorder.ErrorRecorded())


if __name__ == '__main__':
  unittest.main()
