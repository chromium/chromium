#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import unittest
import sys

from presubmit_support import _CheckNoDirectLitImport, _CheckBannedTsTags

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi, MockFile


class NoDirectLitImportPresubmit(unittest.TestCase):
    def setUp(self):
        self.mock_input_api = MockInputApi()
        self.mock_input_api.change.RepositoryRoot = lambda: os.path.join(
            os.path.dirname(__file__), '..', '..', '..')

        self.mock_output_api = MockOutputApi()

    def emulateJsAndTsFiles(self, lines):
        ts_path = os.path.join('some', 'path', 'foo.ts')
        js_path = os.path.join('some', 'path', 'foo.js')
        foo_ts = MockFile(ts_path, lines)
        foo_js = MockFile(js_path, lines)
        self.mock_input_api.files.extend((foo_ts, foo_js))
        return ts_path, js_path

    def testWarningWithDirectLitImport(self):
        """
        If a TS file foo.ts or a JS file foo.js is changed, and there's direct
        Lit import in the file, show warnings.
        """
        lines = [
            "import {aaa} from 'a.js';",
            "import {css} from 'chrome://resources/mwc/lit/index.js';",
        ]
        ts_path, js_path = self.emulateJsAndTsFiles(lines)
        errors = _CheckNoDirectLitImport(self.mock_input_api,
                                         self.mock_output_api)
        self.assertEqual(2, len(errors))
        self.assertTrue(ts_path + ':2' in errors[0].message)
        self.assertTrue(js_path + ':2' in errors[1].message)

    def testNoWarningWithDirectLitImportInXfBase(self):
        """
        If ui/file_manager/file_manager/widgets/xf_base.ts is changed, and
        there's direct lit import in the file, no warnings.
        """
        lines = [
            "import {aaa} from 'a.js';",
            "import {css} from 'chrome://resources/mwc/lit/index.js';",
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
        self.emulateJsAndTsFiles(lines=[])
        errors = _CheckNoDirectLitImport(self.mock_input_api,
                                         self.mock_output_api)
        self.assertEqual([], errors)


class BannedTsTagsTest(unittest.TestCase):
    def setUp(self):
        self.mock_input_api = MockInputApi()
        self.mock_input_api.change.RepositoryRoot = lambda: os.path.join(
            os.path.dirname(__file__), '..', '..', '..')

        self.mock_output_api = MockOutputApi()

    def emulateJsAndTsFiles(self, lines):
        ts_path = os.path.join('some', 'path', 'foo.ts')
        js_path = os.path.join('some', 'path', 'foo.js')
        foo_ts = MockFile(ts_path, lines)
        foo_js = MockFile(js_path, lines)
        self.mock_input_api.files.extend((foo_ts, foo_js))
        return ts_path, js_path

    def testNoTags(self):
        lines = [
            "import {aaa} from 'a.js';",
            "import {css} from 'chrome://resources/mwc/lit/index.js';",
        ]
        self.emulateJsAndTsFiles(lines)
        errors = _CheckBannedTsTags(self.mock_input_api, self.mock_output_api)
        self.assertEqual(0, len(errors))

    def testTagOutsideComment(self):
        lines = [
            "import {aaa} from 'a.js';",
            "@ts-ignore randomly in the code",
        ]
        self.emulateJsAndTsFiles(lines)
        errors = _CheckBannedTsTags(self.mock_input_api, self.mock_output_api)
        self.assertEqual(0, len(errors))

    def testDetectInComments(self):
        lines = [
            "// @ts-ignore: It should block for TS",
            "import {aaa} from 'a.js';",
        ]
        ts_path, js_path = self.emulateJsAndTsFiles(lines)
        errors = _CheckBannedTsTags(self.mock_input_api, self.mock_output_api)
        self.assertEqual(1, len(errors))
        self.assertIn(ts_path, errors[0].items[0])


if __name__ == '__main__':
    unittest.main()
