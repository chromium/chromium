#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for run.py"""

import argparse
import unittest

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


if __name__ == '__main__':
  unittest.main()
