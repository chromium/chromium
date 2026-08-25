#!/usr/bin/env vpython3
# Copyright 2026 Google LLC.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest
from unittest import mock

# Add the repository root to sys.path to access PRESUBMIT_test_mocks
_CHROMIUM_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _CHROMIUM_ROOT not in sys.path:
    sys.path.append(_CHROMIUM_ROOT)

from PRESUBMIT_test_mocks import (  # noqa: E402
    MockAffectedFile,
    MockInputApi,
    MockOutputApi,
)

import PRESUBMIT  # noqa: E402


class MockCommand:
    def __init__(self, name, cmd, kwargs=None, message=None, **k):
        self.name = name
        self.cmd = cmd
        self.kwargs = kwargs or {}
        self.message = message


class ChromiumBidiPresubmitTest(unittest.TestCase):
    def setUp(self):
        self.input_api = MockInputApi()
        self.input_api.presubmit_local_path = os.path.abspath(os.path.dirname(__file__))
        self.input_api.PresubmitLocalPath = lambda: self.input_api.presubmit_local_path
        self.input_api.Command = MockCommand
        self.input_api.RunTests = lambda commands: commands
        self.output_api = MockOutputApi()

    def test_check_ruff(self):
        mock_get_ruff = mock.MagicMock(return_value=["ruff_cmd"])
        self.input_api.canned_checks.GetRuff = mock_get_ruff

        results = PRESUBMIT.CheckRuff(self.input_api, self.output_api)
        self.assertEqual(results, ["ruff_cmd"])
        mock_get_ruff.assert_called_once_with(self.input_api, self.output_api)

    def test_check_eslint_no_matching_files(self):
        self.input_api.files = [
            MockAffectedFile(
                os.path.join(self.input_api.presubmit_local_path, "README.md"),
                ["# Title"],
            ),
            MockAffectedFile(
                os.path.join(self.input_api.presubmit_local_path, "tests", "test.py"),
                ["def foo(): pass"],
            ),
        ]
        results = PRESUBMIT.CheckESLint(self.input_api, self.output_api)
        self.assertEqual(results, [])

    def test_check_eslint_with_ts_file(self):
        self.input_api.files = [
            MockAffectedFile(
                os.path.join(self.input_api.presubmit_local_path, "src", "index.ts"),
                ['console.log("hello");'],
            )
        ]
        results = PRESUBMIT.CheckESLint(self.input_api, self.output_api)
        self.assertEqual(len(results), 1)
        cmd = results[0]
        self.assertIn("ESLint", cmd.name)
        self.assertTrue(any("eslint.js" in arg for arg in cmd.cmd))

    def test_check_eslint_missing_node_modules_on_commit(self):
        self.input_api.files = [
            MockAffectedFile(
                os.path.join(self.input_api.presubmit_local_path, "src", "index.ts"),
                ['console.log("hello");'],
            )
        ]
        self.input_api.is_committing = True
        with mock.patch.object(self.input_api.os_path, "exists", return_value=False):
            results = PRESUBMIT.CheckESLint(self.input_api, self.output_api)
            self.assertEqual(len(results), 1)
            self.assertEqual(results[0].type, "error")
            self.assertIn("node_modules directory is missing", results[0].message)
            self.assertIn("gclient sync", results[0].message)

    def test_check_prettier_with_json_and_ts(self):
        self.input_api.files = [
            MockAffectedFile(
                os.path.join(self.input_api.presubmit_local_path, "package.json"),
                ["{}"],
            ),
            MockAffectedFile(
                os.path.join(self.input_api.presubmit_local_path, "src", "index.ts"),
                ["export {}"],
            ),
        ]
        results = PRESUBMIT.CheckPrettier(self.input_api, self.output_api)
        self.assertEqual(len(results), 1)
        cmd = results[0]
        self.assertIn("Prettier", cmd.name)
        self.assertIn("--check", cmd.cmd)


if __name__ == "__main__":
    unittest.main()
