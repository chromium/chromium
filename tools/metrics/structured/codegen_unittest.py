#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for codegen.py.
"""

# TODO(crbug.com/40156926): Set up these tests to run on the tryjobs.

import unittest
from codegen import Util


class CodegenTest(unittest.TestCase):
  """Tests for codegen.py"""

  def test_sanitize_name(self):
    self.assertEqual(Util.sanitize_name('My Metric.Or::Event/Or/Project_name'),
                     'My_Metric_Or__Event_Or_Project_name', 'chrome')

  def test_camel_to_snake(self):
    def check(camel, expected_snake):
      self.assertEqual(Util.camel_to_snake(camel), expected_snake)

    check('already_snake_case', 'already_snake_case')
    check('ConvertFromCamelCase', 'convert_from_camel_case')
    check('HTTPAcronymAtStart', 'http_acronym_at_start')
    check('AcronymInHTTPMiddle', 'acronym_in_http_middle')
    check('AcronymAtEndHTTP', 'acronym_at_end_http')

  def test_hash_name(self):
    # This was generated using the function in Chromium's
    # //base/metrics/metrics_hashes.cc.
    known_good_hash = 11096769389970233700
    self.assertEqual(Util.hash_name('known good hash'), known_good_hash)

  def test_event_name_hash(self):
    # This was generated using the function in Chromium's
    # //base/metrics/metrics_hashes.cc for the string
    # chrome::TestProjectOne::TestEventOne
    event_name_hash = 13593049295042080097
    project_name = 'TestProjectOne'
    event_name = 'TestEventOne'
    self.assertEqual(Util.event_name_hash(project_name, event_name, 'chrome'),
                     event_name_hash)


if __name__ == '__main__':
  unittest.main()
