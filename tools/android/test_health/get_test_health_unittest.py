#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for the get_test_health script."""

import pathlib
import subprocess
import sys
import tempfile
import unittest

_TOOLS_ANDROID_PATH = pathlib.Path(__file__).resolve(strict=True).parents[1]
if str(_TOOLS_ANDROID_PATH) not in sys.path:
    sys.path.append(str(_TOOLS_ANDROID_PATH))
from python_utils import git_metadata_utils
from python_utils import subprocess_utils

_GET_TEST_HEALTH_PATH = (pathlib.Path(__file__).parent /
                         'get_test_health.py').resolve(strict=True)

_CHROMIUM_SRC_PATH = git_metadata_utils.get_chromium_src_path()
_TEST_FILES_PATH = (_CHROMIUM_SRC_PATH / 'tools' / 'android' / 'test_health' /
                    'testdata' / 'javatests').relative_to(_CHROMIUM_SRC_PATH)


class GetTestHealthTests(unittest.TestCase):
    def test_gettesthealth_writes_to_output_file(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            json_file = pathlib.Path(tmpdir) / 'test_output'

            subprocess_utils.run_command([
                str(_GET_TEST_HEALTH_PATH), '--output-file',
                str(json_file), '--test-dir',
                str(_TEST_FILES_PATH)
            ])

            self.assertTrue(json_file.exists())
            with open(json_file) as f:
                json_lines = f.readlines()
            self.assertGreater(len(json_lines), 0)

    def test_gettesthealth_output_file_not_specified(self):
        with self.assertRaises(subprocess.CalledProcessError) as error_cm:
            subprocess_utils.run_command([
                str(_GET_TEST_HEALTH_PATH), '--test-dir',
                str(_TEST_FILES_PATH)
            ])

        self.assertGreater(error_cm.exception.returncode, 0)
        self.assertIn('the following arguments are required: -o/--output-file',
                      error_cm.exception.stderr)


if __name__ == '__main__':
    unittest.main()
