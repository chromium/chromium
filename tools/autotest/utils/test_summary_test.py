#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile
import unittest
import json

from test_summary import TestSummary, ParseTests


class TestSummaryTest(unittest.TestCase):

  def test_str_empty(self):
    summary = TestSummary()
    expected = "Test count: \n0\n\nPassed tests:\n\n\nFailed tests:\n"
    self.assertEqual(str(summary), expected)

  def test_str_filled(self):
    summary = TestSummary(test_count=2,
                          failed_tests=[(
                              "test_b",
                              "AssertionError\n  File x.py",
                          )],
                          passed_tests=["test_a", "test_c"])
    expected = ("Test count: \n2\n\n"
                "Passed tests:\n"
                "[1/2] test_a\n"
                "[2/2] test_c\n\n"
                "Failed tests:\n"
                "[1/1] test_b\n"
                "    AssertionError\n"
                "      File x.py")
    self.assertEqual(str(summary), expected)

  def test_str_error(self):
    summary = TestSummary(parse_error="Some error")
    expected = ("Parse error: Some error\n\n"
                "Test count: \n0\n\n"
                "Passed tests:\n\n\n"
                "Failed tests:\n")
    self.assertEqual(str(summary), expected)


class ParseTestsTest(unittest.TestCase):

  def setUp(self):
    super().setUp()
    self.fd, self.temp_file = tempfile.mkstemp()

  def tearDown(self):
    os.close(self.fd)
    if os.path.exists(self.temp_file):
      os.remove(self.temp_file)
    super().tearDown()

  def write_content(self, data):
    with open(self.temp_file, 'w') as f:
      if isinstance(data, str):
        f.write(data)
      else:
        json.dump(data, f)

  def test_missing_file(self):
    summary = ParseTests("non_existent_file.json")
    self.assertEqual(summary.test_count, 0)
    self.assertEqual(summary.parse_error, "Summary file is missing or empty")
    self.assertEqual(summary.failed_tests, [])
    self.assertEqual(summary.passed_tests, [])

  def test_empty_file(self):
    self.write_content("")
    summary = ParseTests(self.temp_file)
    self.assertEqual(summary.test_count, 0)
    self.assertEqual(summary.parse_error, "Summary file is missing or empty")
    self.assertEqual(summary.failed_tests, [])
    self.assertEqual(summary.passed_tests, [])

  def test_invalid_json(self):
    self.write_content("{invalid_json:")
    summary = ParseTests(self.temp_file)
    self.assertEqual(summary.test_count, 0)
    self.assertTrue(
        summary.parse_error.startswith("Failed to parse summary file as JSON"))
    self.assertEqual(summary.failed_tests, [])
    self.assertEqual(summary.passed_tests, [])

  def test_valid_json_mixed_results(self):
    data = {
        "per_iteration_data": [{
            "test_pass": [{
                "status": "SUCCESS"
            }],
            "test_fail": [{
                "status": "FAILURE",
                "output_snippet": "error log"
            }],
            "test_crash": [{
                "status": "CRASH",
                "output_snippet": "crash log"
            }],
            "test_timeout": [{
                "status": "TIMEOUT",
                "output_snippet": "timeout log"
            }],
            "test_skipped": [{
                "status": "SKIPPED"
            }],
            "test_flaky": [{
                "status": "FAILURE",
                "output_snippet": "flake log 1"
            }, {
                "status": "SUCCESS"
            }]
        }]
    }
    self.write_content(data)
    summary = ParseTests(self.temp_file)
    self.assertEqual(summary.test_count, 6)

    self.assertEqual(
        sorted(summary.failed_tests),
        sorted([("test_fail", "error log"), ("test_crash", "crash log"),
                ("test_timeout", "timeout log"),
                ("test_flaky", "flake log 1")]))
    self.assertEqual(summary.passed_tests, ["test_pass", "test_skipped"])

  def test_multiple_iterations(self):
    data = {
        "per_iteration_data": [{
            "test_1": [{
                "status": "SUCCESS"
            }],
        }, {
            "test_1": [{
                "status": "SUCCESS"
            }],
            "test_2": [{
                "status": "FAILURE",
                "output_snippet": "err"
            }]
        }]
    }
    self.write_content(data)
    summary = ParseTests(self.temp_file)

    self.assertEqual(summary.test_count, 3)
    self.assertEqual(summary.passed_tests, ["test_1"])
    self.assertEqual(summary.failed_tests, [("test_2", "err")])


if __name__ == '__main__':
  unittest.main()
