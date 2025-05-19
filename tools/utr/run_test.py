#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for run.py"""

import argparse
import logging
import unittest
from unittest import mock

import run


class ParseArgsTest(unittest.TestCase):

  def testRunModes(self):
    argv = [
        '-B',
        'bucket',
        '-b',
        'builder',
        '-t',
        'some_test',
        'compile',
    ]
    args = run.parse_args(argv)
    self.assertEqual(args.run_mode, 'compile')

    argv = [
        '-B',
        'bucket',
        '-b',
        'builder',
        '-t',
        'some_test',
        'wrong-mode',
    ]
    # Invalid run_mode choice should make the parser error out.
    with self.assertRaises(SystemExit):
      args = run.parse_args(argv)

    argv = [
        '-B',
        'bucket',
        '-b',
        'builder',
        '-t',
        'some_test',
    ]
    # No run_mode choice should make the parser error out.
    with self.assertRaises(SystemExit):
      args = run.parse_args(argv)

  def testPrintSurvey(self):
    patch_random = mock.patch('random.random')
    mock_random = patch_random.start()
    mock_random.return_value = 0.005
    with self.assertLogs() as info_log:
      run.maybe_print_survey_link()
      self.assertIn(
          'INFO:root:Help us improve by sharing your feedback in this short '
          'survey: https://forms.gle/tA41evzW5goqR5WF9', info_log.output)

    # No logs to the mock result in an exception trying to assert
    # Verify nothing new is logged instead
    mock_random.return_value = 0.051
    with self.assertLogs() as info_log:
      logging.info('')
      run.maybe_print_survey_link()
      self.assertEqual(
          ['INFO:root:'],
          info_log.output,
      )
    mock_random.stop()

  def testTests(self):
    argv = [
        '-B',
        'bucket',
        '-b',
        'builder',
        'test',
    ]
    # No test should make the parser error out.
    with self.assertRaises(SystemExit):
      args = run.parse_args(argv)

    argv = [
        '-B',
        'bucket',
        '-b',
        'builder',
        '-t',
        'test1',
        '-t',
        'test2',
        'test',
        '--',
        '--gtest_repeat=100',
    ]
    args = run.parse_args(argv)
    self.assertEqual(args.tests, ['test1', 'test2'])
    self.assertEqual(args.run_mode, 'test')
    self.assertEqual(args.additional_test_args, ['--gtest_repeat=100'])

  def testProject(self):
    argv = [
        '-p',
        'some-weird-project',
        '-B',
        'bucket',
        '-b',
        'builder',
        '-t',
        'some_test',
        'compile',
    ]
    # Unknown project should make the parser error out.
    with self.assertRaises(SystemExit):
      args = run.parse_args(argv)

    argv = [
        '-p',
        'chrome-m123xyz',
        '-B',
        'bucket',
        '-b',
        'builder',
        '-t',
        'some_test',
        'compile',
    ]
    # Wrong milestone project format should make the parser error out.
    with self.assertRaises(SystemExit):
      args = run.parse_args(argv)

    argv = [
        '-p',
        'chromium-m123',
        '-B',
        'bucket',
        '-b',
        'builder',
        '-t',
        'some_test',
        'compile',
    ]
    # 'chromium123' should get corrected to just 'chromium'.
    args = run.parse_args(argv)
    self.assertEqual(args.project, 'chromium')

    argv = [
        '-p',
        'chromium',
        '-B',
        'bucket',
        '-b',
        'builder',
        '-t',
        'some_test',
        '-d',
        'foo:bar',
        'compile',
    ]
    # Dimensions with 'compile' should error out.
    with self.assertRaises(SystemExit):
      args = run.parse_args(argv)

    argv = [
        '-p',
        'chromium',
        '-B',
        'bucket',
        '-b',
        'builder',
        '-t',
        'some_test',
        '-d',
        'foobar',
        'test',
    ]
    # Bad dimension arg should error out.
    with self.assertRaises(SystemExit):
      args = run.parse_args(argv)

    argv = [
        '-p',
        'chromium',
        '-B',
        'bucket',
        '-b',
        'builder',
        '-t',
        'some_test',
        '-d',
        'foo=bar',
        'test',
    ]
    # Key=Value dimension passes the regex.
    args = run.parse_args(argv)
    self.assertEqual(args.dimensions, ['foo=bar'])

    argv = [
        '-p',
        'chromium',
        '-B',
        'bucket',
        '-b',
        'builder',
        '-t',
        'some_test',
        '-d',
        'foo=',
        'test',
    ]
    # Key= dimension passes the regex.
    args = run.parse_args(argv)
    self.assertEqual(args.dimensions, ['foo='])

    argv = [
        '-p',
        'chromium',
        '-B',
        'bucket',
        '-b',
        'builder',
        '-t',
        'some_test',
        '-d',
        'foo=bar=baz',
        'test',
    ]
    # Accept dimension value with =.
    args = run.parse_args(argv)
    self.assertEqual(args.dimensions, ['foo=bar=baz'])


if __name__ == '__main__':
  unittest.main()
