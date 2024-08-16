#!/usr/bin/env vpython3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile
import unittest

import orderfile_generator_backend

class TestOrderfileGenerator(unittest.TestCase):
  def testStepRecorder(self):
    """Checks that the step recorder records step timings correctly."""
    step_recorder = orderfile_generator_backend.StepRecorder()
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

  def testGetFileExtension(self):
    self.assertEqual('zip',
        orderfile_generator_backend._GetFileExtension('/foo/bar/baz.blub.zip'))

  def testGenerateHash(self):
    try:
      with tempfile.NamedTemporaryFile(mode='w', delete=False) as handle:
        filename = handle.name
        handle.write('foo')
      self.assertEqual('0beec7b5ea3f0fdbc95d0dd47f3c5bc275da8a33',
                       orderfile_generator_backend._GenerateHash(filename))
    finally:
      if filename:
        os.unlink(filename)


if __name__ == '__main__':
  unittest.main()
