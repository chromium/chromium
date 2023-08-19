#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for test_health_exporter."""

import datetime as dt
import json
import pathlib
import tempfile
import unittest

from java_test_utils import JavaTestHealth
import test_health_exporter
from test_health_extractor import GitRepoInfo, TestHealthInfo

_TEST_TYPE = 'JAVA'
_JAVA_PACKAGE = 'org.chromium.foo_pkg'
_GIT_HEAD_HASH = 'fcd260583cb6d2c739b91f53363fcf0ad3eb3216'
_GIT_HEAD_TIME = '2022-01-18T22:45:48.000000+00:00'
_GIT_REPO_INFO = GitRepoInfo(
    git_head=_GIT_HEAD_HASH,
    git_head_time=dt.datetime.fromisoformat(_GIT_HEAD_TIME))

_JAVA_TEST_NAME = 'FooTest'
_JAVA_TEST_FILENAME = _JAVA_TEST_NAME + '.java'
_JAVA_TEST_DIR = 'javatests/org/chromium/foo_pkg'

_JAVA_TEST_HEALTH = JavaTestHealth(java_package=_JAVA_PACKAGE,
                                   disabled_tests_count=1,
                                   disable_if_tests_count=2,
                                   tests_count=10)
_TEST_HEALTH_INFO = TestHealthInfo(_JAVA_TEST_NAME,
                                   test_dir=pathlib.Path(_JAVA_TEST_DIR),
                                   test_filename=_JAVA_TEST_FILENAME,
                                   java_test_health=_JAVA_TEST_HEALTH,
                                   git_repo_info=_GIT_REPO_INFO)

_TEST_HEALTH_JSON_DICT = dict(test_name=_JAVA_TEST_NAME,
                              test_path=_JAVA_TEST_DIR,
                              test_filename=_JAVA_TEST_FILENAME,
                              test_type=_TEST_TYPE,
                              java_package=_JAVA_PACKAGE,
                              disabled_tests_count=1,
                              disable_if_tests_count=2,
                              tests_count=10,
                              git_head_hash=_GIT_HEAD_HASH,
                              git_head_timestamp=_GIT_HEAD_TIME)


class ToJsonFile(unittest.TestCase):
    """Tests for the to_json_file function."""

    def test_to_json_file_single_java_test(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            json_file = pathlib.Path(tmpdir) / 'test_output'

            test_health_exporter.to_json_file([_TEST_HEALTH_INFO], json_file)

            with open(json_file) as output_file:
                json_lines = output_file.readlines()

        self.assertEqual(1, len(json_lines))

        result = json.loads(json_lines[0])

        self.assertDictEqual(_TEST_HEALTH_JSON_DICT, result)

    def test_to_json_file_multiple_java_tests(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            json_file = pathlib.Path(tmpdir) / 'test_output'

            test_health_exporter.to_json_file(
                [_TEST_HEALTH_INFO, _TEST_HEALTH_INFO, _TEST_HEALTH_INFO],
                json_file)

            with open(json_file) as output_file:
                json_lines = output_file.readlines()

        self.assertEqual(3, len(json_lines))

        result1 = json.loads(json_lines[0])
        result2 = json.loads(json_lines[1])
        result3 = json.loads(json_lines[2])

        self.assertDictEqual(_TEST_HEALTH_JSON_DICT, result1)
        self.assertDictEqual(_TEST_HEALTH_JSON_DICT, result2)
        self.assertDictEqual(_TEST_HEALTH_JSON_DICT, result3)

    def test_to_json_file_java_package_omitted(self):
        java_test_health = JavaTestHealth(java_package=None,
                                          disabled_tests_count=1,
                                          disable_if_tests_count=2,
                                          tests_count=10)
        test_health_info = TestHealthInfo(
            _JAVA_TEST_NAME,
            test_dir=pathlib.Path(_JAVA_TEST_DIR),
            test_filename=_JAVA_TEST_FILENAME,
            java_test_health=java_test_health,
            git_repo_info=_GIT_REPO_INFO)
        test_health_json_dict = _TEST_HEALTH_JSON_DICT.copy()
        del test_health_json_dict['java_package']

        with tempfile.TemporaryDirectory() as tmpdir:
            json_file = pathlib.Path(tmpdir) / 'test_output'

            test_health_exporter.to_json_file([test_health_info], json_file)

            with open(json_file) as output_file:
                json_lines = output_file.readlines()

        self.assertEqual(1, len(json_lines))

        result = json.loads(json_lines[0])

        self.assertDictEqual(test_health_json_dict, result)


if __name__ == '__main__':
    unittest.main()
