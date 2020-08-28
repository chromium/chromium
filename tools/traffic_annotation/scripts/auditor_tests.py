#!/usr/bin/env python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Tests for the Traffic Annotation Auditor.
"""

import os
import sys
import argparse
import unittest
import auditor as auditor_py
from google.protobuf.json_format import ParseError

# Absolute path to chrome/src.
SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
SRC_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "../../.."))
TESTS_DIR = os.path.join(SCRIPT_DIR, "test_data")

# Set via the command line
build_path = ""


class TestTrafficAnnotationAuditor(unittest.TestCase):
  def setUp(self):
    unittest.TestCase.setUp(self)

    path_filters = [os.path.relpath(
      os.path.join(TESTS_DIR, "test_sample_annotations.cc"), SRC_DIR)]
    all_annotations = auditor_py.TrafficAnnotationAuditor.run_extractor(
      path_filters)

    self.auditor_ui = auditor_py.TrafficAnnotationAuditorUI(
      build_path, path_filters, False)

    self.auditor = auditor_py.TrafficAnnotationAuditor()

    self.sample_annotations = {}
    for annotation in all_annotations:
      self.sample_annotations[annotation.unique_id] = annotation

  def test_setup(self):
    """|self.sample_annotations| should include all those inside
    test_data/test_sample_annotations.cc"""
    expected = [
      "ok_annotation", "syntax_error_annotation", "incomplete_error_annotation"]
    self.assertItemsEqual(expected, self.sample_annotations.keys())

  def test_ensure_errors(self):
    """In the |test_sample_annotations.cc| there are some broken annotations.

    This test ensures that TrafficAnnotationAuditorUI catches these errors by
    running from start to finish via |.main()|
    """
    # Suppress |self.auditor_ui.main()| prints to stdout.
    sys.stdout = open(os.devnull, "w")
    self.assertEqual(1, self.auditor_ui.main())   # 1 indicates errors caught.
    sys.stdout = sys.__stdout__

  def test_result_ok(self):
    self.auditor.parse_extractor_output(
      [self.sample_annotations["ok_annotation"]])

    # Assert that correct annotation has been extracted and is OK (no errors).
    self.assertTrue(self.auditor.extracted_annotations)
    self.assertFalse(self.auditor.errors)

  def test_syntax_error(self):
    self.auditor.parse_extractor_output(
      [self.sample_annotations["syntax_error_annotation"]])

    self.assertTrue(self.auditor.errors)
    result = self.auditor.errors[0]
    self.assertEqual(auditor_py.AuditorError.Type.ERROR_SYNTAX, result.type)
    self.assertTrue(
      "sender: \"Cloud Policy\"': Expected \"{\"" in str(result))

  def test_incomplete_error(self):
    self.auditor.parse_extractor_output(
      [self.sample_annotations["incomplete_error_annotation"]])

    self.assertTrue(self.auditor.extracted_annotations)
    self.auditor.run_all_checks()
    self.assertTrue(self.auditor.errors)
    result = self.auditor.errors[0]
    self.assertEqual(
      auditor_py.AuditorError.Type.ERROR_INCOMPLETE_ANNOTATION, result.type)

    expected_missing_fields = [
      "sender", "chrome_policy", "cookies_store",
      "policy_exception_justification"]
    missing_fields = str(result).split(
      "missing fields:", 1)[1].lstrip().split(", ")
    self.assertItemsEqual(expected_missing_fields, missing_fields)


if __name__ == "__main__":
  args_parser = argparse.ArgumentParser(
    description="Unittests for auditor.py")
  args_parser.add_argument(
    "--build-path",
    help="Path to the build directory.",
    default=os.path.join(SRC_DIR, "out/Default"))
  args_parser.add_argument('unittest_args', nargs='*')

  args = args_parser.parse_args()
  build_path = args.build_path
  sys.argv[1:] = args.unittest_args
  unittest.main()