#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from unittest import mock
import upload


class TestCheckPresubmitResults(unittest.TestCase):
  @mock.patch('sys.stdin.isatty', return_value=True)
  @mock.patch('builtins.input', return_value='y')
  def test_tty_prompt_yes_continues(self, mock_input, mock_isatty):
    results = {'warnings': [{'message': 'warn'}]}
    upload.check_presubmit_results(results, allow_warnings=False)
    mock_input.assert_called_once_with(
      'There were presubmit warnings. Are you sure you wish to continue? (y/N):'
    )

  @mock.patch('sys.stdin.isatty', return_value=True)
  @mock.patch('builtins.input', return_value='n')
  def test_tty_prompt_no_aborts(self, mock_input, mock_isatty):
    results = {'warnings': [{'message': 'warn'}]}
    with self.assertRaises(SystemExit):
      upload.check_presubmit_results(results, allow_warnings=False)

  @mock.patch('sys.stdin.isatty', return_value=False)
  def test_non_tty_aborts(self, mock_isatty):
    results = {'warnings': [{'message': 'warn'}]}
    with self.assertRaises(SystemExit):
      upload.check_presubmit_results(results, allow_warnings=False)


if __name__ == '__main__':
  unittest.main()
