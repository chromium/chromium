#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for rebase_dump_accessibility_tree_test.py"""

import unittest
import rebase_dump_accessibility_tree_tests as rebase


class RebaseDumpAccessibilityTreeTestTests(unittest.TestCase):

  def test_split_test_logs(self):
    log = """
Testing: file1.txt
file1 content
<-- End-of-file -->

Testing: file2.txt
file2 has different content
<-- End-of-file -->
    """.split("\n")

    actual = rebase.SplitTestLogs(log)
    expected = [
        [
            "Testing: file1.txt",
            "file1 content",
            "<-- End-of-file -->",
        ],
        [
            "Testing: file2.txt",
            "file2 has different content",
            "<-- End-of-file -->",
        ],
    ]
    self.assertEqual(actual, expected)

  def test_parse_log(self):
    log = """
Testing: content/test/data/accessibility/test_file
Expected output: content/test/data/accessibility/expectation_file
Diff:
* Line Expected
- ---- --------
some oudated expectations

Actual
------
actual file content
<-- End-of-file -->
""".split("\n")
    filename, data = rebase.ParseLog(log)

    self.assertEqual(filename, "expectation_file")
    self.assertEqual(data, "actual file content\n")


if __name__ == "__main__":
  unittest.main()
