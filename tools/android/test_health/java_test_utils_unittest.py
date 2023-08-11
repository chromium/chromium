#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for java_test_utils."""

import pathlib
import sys
import unittest

import java_test_utils

_TOOLS_ANDROID_PATH = pathlib.Path(__file__).resolve(
    strict=True).parents[1].resolve(strict=True)
if str(_TOOLS_ANDROID_PATH) not in sys.path:
    sys.path.append(str(_TOOLS_ANDROID_PATH))
from python_utils import git_metadata_utils

_CHROMIUM_SRC_PATH = git_metadata_utils.get_chromium_src_path()

_TEST_FILES_PATH = (pathlib.Path(__file__).parents[0] / 'testdata' /
                    'javatests' / 'org' / 'chromium' / 'chrome' / 'browser' /
                    'test_health').resolve(strict=True)

_HEALTHY_TEST_PATH = _TEST_FILES_PATH / 'healthy_tests' / 'SampleTest.java'
_HEALTHY_NO_PKG_TEST_PATH = (_TEST_FILES_PATH / 'healthy_tests' /
                             'SampleNoPackageTest.java')
_UNHEALTHY_TEST_PATH = (_TEST_FILES_PATH / 'unhealthy_tests' /
                        'SampleTest.java')
_INVALID_SYNTAX_TEST_PATH = (_TEST_FILES_PATH / 'unhealthy_tests' /
                             'InvalidSyntaxTest.java')
_DISABLED_TEST_PATH = (_TEST_FILES_PATH / 'disabled_tests' /
                       'SampleDisabledTest.java')
_DISABLE_IF_TEST_PATH = (_TEST_FILES_PATH / 'disabled_tests' /
                         'SampleDisableIfTest.java')
_WHOLE_CLASS_DISABLED_TEST_PATH = (_TEST_FILES_PATH / 'disabled_tests' /
                       'SampleWholeClassDisabledTest.java')

_BASE_JAVA_PACKAGE = 'org.chromium.chrome.browser.test_health'
_JAVA_PACKAGE_HEALTHY_TESTS = _BASE_JAVA_PACKAGE + '.healthy_tests'
_JAVA_PACKAGE_UNHEALTHY_TESTS = _BASE_JAVA_PACKAGE + '.unhealthy_tests'
_JAVA_PACKAGE_DISABLED_TESTS = _BASE_JAVA_PACKAGE + '.disabled_tests'


class TestJavaTestHealthStats(unittest.TestCase):
    """Tests for the get_java_test_health_stats function."""

    def test_get_java_test_health_stats_healthy_tests(self):
        test_health = java_test_utils.get_java_test_health(_HEALTHY_TEST_PATH)

        self.assertEqual(_JAVA_PACKAGE_HEALTHY_TESTS, test_health.java_package)
        self.assertEqual(0, test_health.disabled_tests_count)
        self.assertEqual(0, test_health.disable_if_tests_count)
    def test_get_java_test_health_stats_healthy_tests_no_java_package(self):
        test_health = java_test_utils.get_java_test_health(
            _HEALTHY_NO_PKG_TEST_PATH)

        self.assertIsNone(test_health.java_package)
        self.assertEqual(0, test_health.disabled_tests_count)
        self.assertEqual(0, test_health.disable_if_tests_count)

    def test_get_java_test_health_stats_unhealthy_tests(self):
        test_health = java_test_utils.get_java_test_health(
            _UNHEALTHY_TEST_PATH)

        self.assertEqual(_JAVA_PACKAGE_UNHEALTHY_TESTS,
                         test_health.java_package)
        self.assertEqual(1, test_health.disabled_tests_count)
        self.assertEqual(1, test_health.disable_if_tests_count)

    def test_get_java_test_health_stats_disabled_tests(self):
        test_health = java_test_utils.get_java_test_health(_DISABLED_TEST_PATH)

        self.assertEqual(_JAVA_PACKAGE_DISABLED_TESTS,
                         test_health.java_package)
        self.assertEqual(2, test_health.disabled_tests_count)
        self.assertEqual(0, test_health.disable_if_tests_count)

    def test_get_java_test_health_stats_disable_if_tests(self):
        test_health = java_test_utils.get_java_test_health(
            _DISABLE_IF_TEST_PATH)

        self.assertEqual(_JAVA_PACKAGE_DISABLED_TESTS,
                         test_health.java_package)
        self.assertEqual(0, test_health.disabled_tests_count)
        self.assertEqual(2, test_health.disable_if_tests_count)

    def test_get_java_test_health_stats_whole_class_disabled_tests(self):
        test_health = java_test_utils.get_java_test_health(_WHOLE_CLASS_DISABLED_TEST_PATH)

        self.assertEqual(_JAVA_PACKAGE_DISABLED_TESTS,
                         test_health.java_package)
        self.assertEqual(2, test_health.disabled_tests_count)
        self.assertEqual(0, test_health.disable_if_tests_count)

    def test_get_java_test_health_invalid_test_syntax(self):
        expected_filename = str(
            _INVALID_SYNTAX_TEST_PATH.relative_to(_CHROMIUM_SRC_PATH))
        expected_text = ('        values = Arrays.stream(STRING_ARRAY_2D)'
                         '.map(String[] ::clone).toArray(String[][] ::new);')

        with self.assertRaises(java_test_utils.JavaSyntaxError) as error_cm:
            java_test_utils.get_java_test_health(_INVALID_SYNTAX_TEST_PATH)

        self.assertEqual("Expected '.'", error_cm.exception.msg)
        self.assertEqual(expected_filename, error_cm.exception.filename)
        self.assertEqual(30, error_cm.exception.lineno)
        self.assertEqual(64, error_cm.exception.offset)
        self.assertEqual(expected_text, error_cm.exception.text)


if __name__ == '__main__':
    unittest.main()
