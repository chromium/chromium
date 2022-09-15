# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for is_flaky."""

import is_flaky
import subprocess
import sys
import threading
import unittest


class IsFlakyTest(unittest.TestCase):

  def setUp(self):
    self.original_subprocess_check_call = subprocess.check_call
    subprocess.check_call = self.mock_check_call
    self.check_call_calls = []
    self.check_call_results = []
    is_flaky.load_options = self.mock_load_options

  def tearDown(self):
    subprocess.check_call = self.original_subprocess_check_call

  def mock_check_call(self, command, stdout, stderr):
    self.check_call_calls.append(command)
    if self.check_call_results:
      return self.check_call_results.pop(0)
    else:
      return 0

  def mock_load_options(self):
    class MockOptions():
      jobs = 2
      retries = 10
      threshold = 0.3
      command = ['command', 'param1', 'param2']
    return MockOptions()

  def testExecutesTestCorrectNumberOfTimes(self):
    is_flaky.main()
    self.assertEqual(len(self.check_call_calls), 10)

  def testExecutesTestWithCorrectArguments(self):
    is_flaky.main()
    for call in self.check_call_calls:
      self.assertEqual(call, ['command', 'param1', 'param2'])

  def testReturnsNonFlakyForAllSuccesses(self):
    self.check_call_results = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
    ret_code = is_flaky.main()
    self.assertEqual(ret_code, 0)

  def testReturnsNonFlakyForAllFailures(self):
    self.check_call_results = [1, 1, 1, 1, 1, 1, 1, 1, 1, 1]
    ret_code = is_flaky.main()
    self.assertEqual(ret_code, 0)

  def testReturnsNonFlakyForSmallNumberOfFailures(self):
    self.check_call_results = [1, 0, 1, 0, 0, 0, 0, 0, 0, 0]
    ret_code = is_flaky.main()
    self.assertEqual(ret_code, 0)

  def testReturnsFlakyForLargeNumberOfFailures(self):
    self.check_call_results = [1, 1, 1, 0, 1, 0, 0, 0, 0, 0]
    ret_code = is_flaky.main()
    self.assertEqual(ret_code, 1)


if __name__ == '__main__':
  unittest.main()
