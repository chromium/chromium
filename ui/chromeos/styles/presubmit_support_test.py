#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import subprocess
import unittest
import sys
from presubmit_support import _CheckSemanticColors

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi, MockFile

# CSS variable used throughout the tests as valid.
_CANONICAL_VALID_VARIABLE = '--cros-bg-color'


class CSSVariablePresubmitBase(unittest.TestCase):
    def setUp(self):
        self.mock_input_api = MockInputApi()
        self.mock_input_api.change.RepositoryRoot = lambda: os.path.join(
            os.path.dirname(__file__), '..', '..', '..')

        self.mock_output_api = MockOutputApi()

    def mockFileWithLines(self, file_path, lines):
        mock_file = MockFile(file_path, lines)
        self.mock_input_api.files.append(mock_file)


class CSSVariableContext(CSSVariablePresubmitBase):
    def testUnknownCSSContext(self):
        lines = [
            'selector {', '\tcolor: var(--unknown-should-be-ignored);', '}'
        ]
        self.mockFileWithLines('some/path/file.css', lines)
        errors = _CheckSemanticColors(self.mock_input_api,
                                      self.mock_output_api)
        self.assertEqual(0, len(errors))

    def testKnownCSSContext(self):
        lines = ['selector {', '\tcolor: var(--cros-should-be-flagged);', '}']
        self.mockFileWithLines('some/path/file.css', lines)
        errors = _CheckSemanticColors(self.mock_input_api,
                                      self.mock_output_api)
        self.assertEqual(1, len(errors))
        self.assertEqual(1, len(errors[0].items))


class CSSVariableValidity(CSSVariablePresubmitBase):
    def testMultipleUnknownVariables(self):
        lines = [
            'selector {'
            '\tcolor: var(--cros-unknown-1);',
            '\tbackground-color: var(--cros-unknown-2);',
            '\tborder: 1px solid var(--cros-unknown-3);', '}'
        ]
        self.mockFileWithLines('some/path/file.css', lines)
        errors = _CheckSemanticColors(self.mock_input_api,
                                      self.mock_output_api)
        self.assertEqual(1, len(errors))
        self.assertEqual(3, len(errors[0].items))

    def testMixKnownUnkownVariables(self):
        lines = [
            'selector {'
            '\tcolor: var(' + _CANONICAL_VALID_VARIABLE + ');',
            '\tbackground-color: var(--cros-unknown-2);',
            '\tborder: 1px solid var(--cros-unknown-3);', '}'
        ]
        self.mockFileWithLines('some/path/file.css', lines)
        errors = _CheckSemanticColors(self.mock_input_api,
                                      self.mock_output_api)
        self.assertEqual(1, len(errors))
        self.assertEqual(2, len(errors[0].items))

    def testMultipleValidatedFilesInvalidVariable(self):
        lines = ['selector {' '\tcolor: var(--cros-invalid-variable);', '}']
        self.mockFileWithLines('some/path/file.css', lines)
        self.mockFileWithLines('some/path/file.html', lines)
        self.mockFileWithLines('some/path/file.js', lines)
        errors = _CheckSemanticColors(self.mock_input_api,
                                      self.mock_output_api)
        self.assertEqual(1, len(errors))
        self.assertEqual(3, len(errors[0].items))

    def testCLWithOtherFilesAreIgnored(self):
        lines = ['selector {' '\tcolor: var(--cros-invalid-variable);', '}']
        self.mockFileWithLines('some/path/file.css', lines)
        self.mockFileWithLines('some/path/file.cc', lines)
        self.mockFileWithLines('some/path/file.h', lines)
        self.mockFileWithLines('some/path/file.py', lines)
        errors = _CheckSemanticColors(self.mock_input_api,
                                      self.mock_output_api)
        self.assertEqual(1, len(errors))
        self.assertEqual(1, len(errors[0].items))
        self.assertTrue('file.css' in errors[0].items[0])


if __name__ == '__main__':
    unittest.main()
