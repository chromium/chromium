#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import tempfile
import unittest
import sys

_HERE_DIR = os.path.dirname(__file__)
_SRC_PATH = os.path.normpath(os.path.join(_HERE_DIR, '..', '..', '..', '..'))
_NODE_PATH = os.path.join(_SRC_PATH, 'third_party', 'node')
sys.path.append(_NODE_PATH)

import node


class LitTemplateFormatterTest(unittest.TestCase):

  def setUp(self):
    self._out_dir = tempfile.mkdtemp(dir=_HERE_DIR)

  def tearDown(self):
    shutil.rmtree(self._out_dir)

  def _read_file(self, path):
    with open(path, 'r', encoding='utf-8') as file:
      return file.read()

  def _run_formatter(self, args):
    formatter_script = os.path.join(_HERE_DIR, "lit_template_formatter",
                                    "main.js")
    node.RunNode([formatter_script] + args)

  # When expected_filename is None, compares the formatted output directly
  # against the original source file, verifying that correct formatting remains
  # unaltered.
  def _run_test(self, filename, expected_filename=None):
    src_path = os.path.join(_HERE_DIR, "tests", "lit_template_formatter",
                            filename)
    expected_path = os.path.join(_HERE_DIR, "tests", "lit_template_formatter",
                                 "expected" if expected_filename else "",
                                 expected_filename or filename)

    expected_contents = self._read_file(expected_path)

    # Copy to temp dir for in-place processing
    dest_path = os.path.join(self._out_dir, filename)
    shutil.copy(src_path, dest_path)

    # First run: format the input
    self._run_formatter([dest_path])
    actual_contents = self._read_file(dest_path)
    self.assertMultiLineEqual(expected_contents, actual_contents)

    # Second run (Idempotency check): re-run formatter on output
    self._run_formatter([dest_path])
    idempotent_contents = self._read_file(dest_path)
    self.assertMultiLineEqual(expected_contents, idempotent_contents)

  def testBasicExpressions(self):
    self._run_test("test_basic_expressions.html.ts",
                   "test_basic_expressions.html.ts")

  def testConditionalAndMap(self):
    self._run_test("test_conditional_and_map.html.ts",
                   "test_conditional_and_map.html.ts")

  def testNestedTemplate(self):
    self._run_test("test_nested_template.html.ts",
                   "test_nested_template.html.ts")

  def testWhitespaceSensitiveSiblings(self):
    self._run_test("test_whitespace_sensitive_siblings.html.ts")

  def testJsUnitTests(self):
    test_script = os.path.join(_HERE_DIR, "lit_template_formatter",
                               "lit_template_formatter_test.js")
    node.RunNode([test_script])


if __name__ == "__main__":
  unittest.main()
