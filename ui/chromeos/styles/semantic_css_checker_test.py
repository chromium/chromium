#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

from semantic_css_checker import SemanticCssChecker

# Update system path to src/ so we can access src/PRESUBMIT_test_mocks.py.
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..',
                                 '..'))

from PRESUBMIT_test_mocks import (MockInputApi, MockOutputApi, MockFile,
                                  MockChange, MockAffectedFile)


class SemanticCssCheckerTest(unittest.TestCase):
  def testNonSemanticColors(self):
    mock_input_api = MockInputApi()
    lines = [
      'color: var(--google-grey-500);',
      'color: rgba(var(--google-blue-300-rgb), 0.2);',
      'color: rgb(255, 99, 71);',
      'fill: rgba(255, 99, 71, 0.5);',
      'fill: #00FFFF;',
      'background-color: hsla(9, 100%, 64%, 0.5);',
      'color: hsl(0, 100%, 50%);',
      'background-color: var(--paper-tab-ink);',
    ]
    mock_input_api.files = [
      MockAffectedFile('chrome/test.html', lines),
      MockAffectedFile('chrome/test.css', lines),
    ]

    mock_output_api = MockOutputApi()

    errors = SemanticCssChecker.RunChecks(
        mock_input_api, mock_output_api)
    self.assertEqual(len(lines) * len(mock_input_api.files), len(errors))

  def testSemanticColors(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('chrome/test.html', [
        'color: var(--cros-icon-color-prominent);',
      ]),
    ]

    mock_output_api = MockOutputApi()

    errors = SemanticCssChecker.RunChecks(
        mock_input_api, mock_output_api)
    self.assertEqual(0, len(errors))

  def testExcludedFiles(self):
    """ Only .html and .css files should be processed. """
    mock_input_api = MockInputApi()
    lines =  [
      'color: var(--google-grey-500);',
    ];
    mock_input_api.files = [
      MockAffectedFile('chrome/test.js',
                       lines),
    ]

    mock_output_api = MockOutputApi()

    errors = SemanticCssChecker.RunChecks(
        mock_input_api, mock_output_api)
    self.assertEqual(0, len(errors))

if __name__ == '__main__':
  unittest.main()
