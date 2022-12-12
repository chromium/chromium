#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
from pathlib import Path
import unittest
from unittest.mock import patch
import sys

from presubmit_support import _CheckGM3Counterpart, _CheckNoDirectLitImport

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi, MockFile


class GM3CounterpartPresubmit(unittest.TestCase):
    def setUp(self):
        self.mock_input_api = MockInputApi()
        self.mock_input_api.change.RepositoryRoot = lambda: os.path.join(
            os.path.dirname(__file__), '..', '..', '..')

        self.mock_output_api = MockOutputApi()

    def testWarningWithGM3CounterpartNotChanged(self):
        """
        If a CSS file foo.css is changed, and there's a corresponding
        foo_gm3.css existed but not changed, show warning.
        """
        foo_css = MockFile(os.path.join('some', 'path', 'foo.css'), '')
        self.mock_input_api.files.append(foo_css)
        # Mock Path.is_file call to make sure foo_gm3.css is existed.
        with patch.object(Path, 'is_file') as mock_is_file:
            mock_is_file.return_value = True
            errors = _CheckGM3Counterpart(self.mock_input_api,
                                          self.mock_output_api)
            self.assertEqual(1, len(errors))
            self.assertEqual(1, len(errors[0].items))
            self.assertTrue('foo.css' in errors[0].items[0])
            self.assertTrue('foo_gm3.css' in errors[0].items[0])

    def testNoWarningWithGM3CounterpartChanged(self):
        """
        If a CSS file foo.css is changed, and there's a corresponding
        foo_gm3.css existed also changed, no warnings.
        """
        foo_css = MockFile(os.path.join('some', 'path', 'foo.css'), '')
        foo_gm3_css = MockFile(os.path.join('some', 'path', 'foo_gm3.css'), '')
        self.mock_input_api.files.append(foo_css)
        self.mock_input_api.files.append(foo_gm3_css)
        # Mock Path.is_file call to make sure foo_gm3.css is existed.
        with patch.object(Path, 'is_file') as mock_is_file:
            mock_is_file.return_value = True
            errors = _CheckGM3Counterpart(self.mock_input_api,
                                          self.mock_output_api)
            self.assertEqual([], errors)

    def testNoWarningWithoutGM3Counterpart(self):
        """
        If a CSS file foo.css is changed, and the corresponding foo_gm3.css
        does not existed, no warnings.
        """
        foo_css = MockFile(os.path.join('some', 'path', 'foo.css'), '')
        self.mock_input_api.files.append(foo_css)
        # Mock Path.is_file call to make sure foo_gm3.css is not existed.
        with patch.object(Path, 'is_file') as mock_is_file:
            mock_is_file.return_value = False
            errors = _CheckGM3Counterpart(self.mock_input_api,
                                          self.mock_output_api)
            self.assertEqual([], errors)

    def testNoWarningForNonCSSChange(self):
        """
        If the patch doesn't have any CSS files, no warnings.
        """
        foo_js = MockFile(os.path.join('some', 'path', 'foo.js'), '')
        foo_cpp = MockFile(os.path.join('some', 'path', 'foo.cc'), '')
        self.mock_input_api.files.append(foo_js)
        self.mock_input_api.files.append(foo_cpp)
        errors = _CheckGM3Counterpart(self.mock_input_api,
                                      self.mock_output_api)
        self.assertEqual([], errors)


class NoDirectLitImportPresubmit(unittest.TestCase):
    def setUp(self):
        self.mock_input_api = MockInputApi()
        self.mock_input_api.change.RepositoryRoot = lambda: os.path.join(
            os.path.dirname(__file__), '..', '..', '..')

        self.mock_output_api = MockOutputApi()

    def testWarningWithDirectLitImport(self):
        """
        If a TS file foo.ts or a JS file foo.js is changed, and there's direct
        Lit import in the file, show warnings.
        """
        lines = [
            "import {aaa} from 'a.js';"
            "import {css} from 'chrome://resources/mwc/lit/index.js';"
        ]
        foo_ts = MockFile(os.path.join('some', 'path', 'foo.ts'), lines)
        foo_js = MockFile(os.path.join('some', 'path', 'foo.js'), lines)
        self.mock_input_api.files.append(foo_ts)
        self.mock_input_api.files.append(foo_js)
        errors = _CheckNoDirectLitImport(self.mock_input_api,
                                         self.mock_output_api)
        self.assertEqual(2, len(errors))
        self.assertTrue('some/path/foo.ts:1' in errors[0].message)
        self.assertTrue('some/path/foo.js:1' in errors[1].message)

    def testNoWarningWithDirectLitImportInXfBase(self):
        """
        If ui/file_manager/file_manager/widgets/xf_base.ts is changed, and
        there's direct lit import in the file, no warnings.
        """
        lines = [
            "import {aaa} from 'a.js';"
            "import {css} from 'chrome://resources/mwc/lit/index.js';"
        ]
        xf_base_ts = MockFile(
            os.path.join('ui', 'file_manager', 'file_manager', 'widgets',
                         'xf_base.ts'), lines)
        self.mock_input_api.files.append(xf_base_ts)
        errors = _CheckNoDirectLitImport(self.mock_input_api,
                                         self.mock_output_api)
        self.assertEqual([], errors)

    def testNoWarningWithoutDirectLitImport(self):
        """
        If a TS file foo.ts is changed, and there is no direct Lit import
        in the file, no warnings.
        """
        foo_ts = MockFile(os.path.join('some', 'path', 'foo.ts'), '')
        foo_js = MockFile(os.path.join('some', 'path', 'foo.js'), '')
        self.mock_input_api.files.append(foo_ts)
        self.mock_input_api.files.append(foo_js)
        errors = _CheckGM3Counterpart(self.mock_input_api,
                                      self.mock_output_api)
        self.assertEqual([], errors)


if __name__ == '__main__':
    unittest.main()
