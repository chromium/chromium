#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
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

_SIX_SRC_PATH = (_CHROMIUM_SRC_PATH / 'third_party' / 'six' /
                 'src').resolve(strict=True)
# six is a dependency of javalang
sys.path.insert(0, str(_SIX_SRC_PATH))

_JAVALANG_SRC_PATH = (_CHROMIUM_SRC_PATH / 'third_party' / 'javalang' /
                      'src').resolve(strict=True)
if str(_JAVALANG_SRC_PATH) not in sys.path:
    sys.path.append(str(_JAVALANG_SRC_PATH))
import javalang

_TEST_FILES_PATH = (pathlib.Path(__file__).parents[0] / 'testdata' /
                    'javatests' / 'org' / 'chromium' / 'chrome' / 'browser' /
                    'test_health').resolve(strict=True)

_HEALTHY_TEST_SRC = (_TEST_FILES_PATH / 'healthy_tests' /
                     'SampleTest.java').resolve(strict=True).read_text()
_HEALTHY_TEST_AST = javalang.parse.parse(_HEALTHY_TEST_SRC)
_HEALTHY_NO_PKG_TEST_SRC = (_TEST_FILES_PATH / 'healthy_tests' /
                            'SampleNoPackageTest.java').resolve(
                                strict=True).read_text()
_HEALTHY_NO_PKG_TEST_AST = javalang.parse.parse(_HEALTHY_NO_PKG_TEST_SRC)
_UNHEALTHY_TEST_SRC = (_TEST_FILES_PATH / 'unhealthy_tests' /
                       'SampleTest.java').resolve(strict=True).read_text()
_UNHEALTHY_TEST_AST = javalang.parse.parse(_UNHEALTHY_TEST_SRC)
_DISABLED_TEST_SRC = (_TEST_FILES_PATH / 'disabled_tests' /
                      'SampleDisabledTest.java').resolve(
                          strict=True).read_text()
_DISABLED_TEST_AST = javalang.parse.parse(_DISABLED_TEST_SRC)
_DISABLE_IF_TEST_SRC = (_TEST_FILES_PATH / 'disabled_tests' /
                        'SampleDisableIfTest.java').resolve(
                            strict=True).read_text()
_DISABLE_IF_TEST_AST = javalang.parse.parse(_DISABLE_IF_TEST_SRC)
_FLAKY_TEST_SRC = (_TEST_FILES_PATH / 'flaky_tests' /
                   'SampleFlakyTest.java').resolve(strict=True).read_text()
_FLAKY_TEST_AST = javalang.parse.parse(_FLAKY_TEST_SRC)

_BASE_JAVA_PACKAGE = 'org.chromium.chrome.browser.test_health'
_JAVA_PACKAGE_HEALTHY_TESTS = _BASE_JAVA_PACKAGE + '.healthy_tests'
_JAVA_PACKAGE_UNHEALTHY_TESTS = _BASE_JAVA_PACKAGE + '.unhealthy_tests'
_JAVA_PACKAGE_DISABLED_TESTS = _BASE_JAVA_PACKAGE + '.disabled_tests'
_JAVA_PACKAGE_FLAKY_TESTS = _BASE_JAVA_PACKAGE + '.flaky_tests'


class TestJavaPackageName(unittest.TestCase):
    """Tests for the get_java_package_name function."""

    def test_get_java_package_name(self):
        java_package = java_test_utils.get_java_package_name(_HEALTHY_TEST_AST)

        self.assertEqual(_JAVA_PACKAGE_HEALTHY_TESTS, java_package)

    def test_get_java_package_name_no_package(self):
        java_package = java_test_utils.get_java_package_name(
            _HEALTHY_NO_PKG_TEST_AST)

        self.assertIsNone(java_package)


class TestJavaTestHealthStats(unittest.TestCase):
    """Tests for the get_java_test_health_stats function."""

    def test_get_java_test_health_stats_healthy_tests(self):
        test_health = java_test_utils.get_java_test_health(_HEALTHY_TEST_AST)

        self.assertEqual(_JAVA_PACKAGE_HEALTHY_TESTS, test_health.java_package)
        self.assertEqual(0, test_health.disabled_tests_count)
        self.assertEqual(0, test_health.disable_if_tests_count)
        self.assertEqual(0, test_health.flaky_tests_count)

    def test_get_java_test_health_stats_unhealthy_tests(self):
        test_health = java_test_utils.get_java_test_health(_UNHEALTHY_TEST_AST)

        self.assertEqual(_JAVA_PACKAGE_UNHEALTHY_TESTS,
                         test_health.java_package)
        self.assertEqual(1, test_health.disabled_tests_count)
        self.assertEqual(1, test_health.disable_if_tests_count)
        self.assertEqual(1, test_health.flaky_tests_count)

    def test_get_java_test_health_stats_disabled_tests(self):
        test_health = java_test_utils.get_java_test_health(_DISABLED_TEST_AST)

        self.assertEqual(_JAVA_PACKAGE_DISABLED_TESTS,
                         test_health.java_package)
        self.assertEqual(2, test_health.disabled_tests_count)
        self.assertEqual(0, test_health.disable_if_tests_count)
        self.assertEqual(0, test_health.flaky_tests_count)

    def test_get_java_test_health_stats_disableif_tests(self):
        test_health = java_test_utils.get_java_test_health(
            _DISABLE_IF_TEST_AST)

        self.assertEqual(_JAVA_PACKAGE_DISABLED_TESTS,
                         test_health.java_package)
        self.assertEqual(0, test_health.disabled_tests_count)
        self.assertEqual(2, test_health.disable_if_tests_count)
        self.assertEqual(0, test_health.flaky_tests_count)

    def test_get_java_test_health_stats_flaky_tests(self):
        test_health = java_test_utils.get_java_test_health(_FLAKY_TEST_AST)

        self.assertEqual(_JAVA_PACKAGE_FLAKY_TESTS, test_health.java_package)
        self.assertEqual(0, test_health.disabled_tests_count)
        self.assertEqual(0, test_health.disable_if_tests_count)
        self.assertEqual(2, test_health.flaky_tests_count)


if __name__ == '__main__':
    unittest.main()
