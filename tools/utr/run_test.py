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


if __name__ == '__main__':
  unittest.main()
