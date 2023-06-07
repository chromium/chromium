#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for test_health_extractor."""

import logging
import pathlib
import sys
import unittest

import java_test_utils
import test_health_extractor

_TOOLS_ANDROID_PATH = pathlib.Path(__file__).resolve(strict=True).parents[1]
if str(_TOOLS_ANDROID_PATH) not in sys.path:
    sys.path.append(str(_TOOLS_ANDROID_PATH))
from python_utils import git_metadata_utils

_CHROMIUM_SRC_PATH = git_metadata_utils.get_chromium_src_path()
_CHROMIUM_REPO_INFO = test_health_extractor.GitRepoInfo(
    git_head=git_metadata_utils.get_head_commit_hash(),
    git_head_time=git_metadata_utils.get_head_commit_datetime())

_TEST_FILES_PATH = (_CHROMIUM_SRC_PATH / 'tools' / 'android' / 'test_health' /
                    'testdata' / 'javatests' / 'org' / 'chromium' / 'chrome' /
                    'browser' / 'test_health').relative_to(_CHROMIUM_SRC_PATH)
_HEALTHY_TESTS_PATH = _TEST_FILES_PATH / 'healthy_tests'
_UNHEALTHY_TESTS_PATH = _TEST_FILES_PATH / 'unhealthy_tests'
_INVALID_SYNTAX_TEST_PATH = (
    _UNHEALTHY_TESTS_PATH /
    'InvalidSyntaxTest.java').relative_to(_TEST_FILES_PATH)

_IGNORED_DIRS = ('disabled_tests', 'unhealthy_tests')

_BASE_JAVA_PACKAGE = 'org.chromium.chrome.browser.test_health'
_JAVA_PACKAGE_HEALTHY_TESTS = _BASE_JAVA_PACKAGE + '.healthy_tests'
_JAVA_PACKAGE_UNHEALTHY_TESTS = _BASE_JAVA_PACKAGE + '.unhealthy_tests'


class GetRepoTestHealth(unittest.TestCase):
    """Tests for the get_repo_test_health function."""

    def test_get_test_health_healthy_test(self):
        sample_test_path = _HEALTHY_TESTS_PATH / 'SampleTest.java'
        expected_test_health_info = test_health_extractor.TestHealthInfo(
            test_name=sample_test_path.stem,
            test_dir=sample_test_path.parent,
            test_filename=sample_test_path.name,
            java_test_health=java_test_utils.JavaTestHealth(
                java_package=_JAVA_PACKAGE_HEALTHY_TESTS,
                disabled_tests_count=0,
                disable_if_tests_count=0,
                tests_count=1),
            git_repo_info=_CHROMIUM_REPO_INFO)

        test_health_infos = test_health_extractor.get_repo_test_health(
            test_dir=_TEST_FILES_PATH,
            ignored_dirs=_IGNORED_DIRS,
            ignored_files={
                str((_HEALTHY_TESTS_PATH /
                     'SampleNoPackageTest.java').relative_to(_TEST_FILES_PATH))
            })

        self.assertEqual(expected_test_health_info, test_health_infos[0])

    def test_get_test_health_unhealthy_test(self):
        sample_test_path = _UNHEALTHY_TESTS_PATH / 'SampleTest.java'
        expected_test_health_info = test_health_extractor.TestHealthInfo(
            test_name=sample_test_path.stem,
            test_dir=sample_test_path.parent,
            test_filename=sample_test_path.name,
            java_test_health=java_test_utils.JavaTestHealth(
                java_package=_JAVA_PACKAGE_UNHEALTHY_TESTS,
                disabled_tests_count=1,
                disable_if_tests_count=1,
                tests_count=4),
            git_repo_info=_CHROMIUM_REPO_INFO)

        test_health_infos = test_health_extractor.get_repo_test_health(
            test_dir=_TEST_FILES_PATH,
            ignored_dirs=('disabled_tests', 'healthy_tests'),
            ignored_files={str(_INVALID_SYNTAX_TEST_PATH)})

        self.assertEqual(expected_test_health_info, test_health_infos[0])

    def test_get_test_health_all_files_and_folders_ignored(self):
        test_health_info = test_health_extractor.get_repo_test_health(
            test_dir=_TEST_FILES_PATH,
            ignored_dirs=_IGNORED_DIRS,
            ignored_files={
                str((_HEALTHY_TESTS_PATH /
                     'SampleTest.java').relative_to(_TEST_FILES_PATH)),
                str((_HEALTHY_TESTS_PATH /
                     'SampleNoPackageTest.java').relative_to(_TEST_FILES_PATH))
            })

        self.assertListEqual([], test_health_info)

    def test_get_test_health_invalid_syntax_test_skipped(self):
        with self.assertLogs(level=logging.WARNING) as logging_cm:
            test_health_infos = test_health_extractor.get_repo_test_health(
                test_dir=_TEST_FILES_PATH,
                ignored_dirs=('disabled_tests', 'healthy_tests'),
                ignored_files={
                    str((_UNHEALTHY_TESTS_PATH /
                         'SampleTest.java').relative_to(_TEST_FILES_PATH))
                })

        self.assertListEqual([], test_health_infos)
        self.assertIn(str(_INVALID_SYNTAX_TEST_PATH), logging_cm.output[0])

    def test_get_test_health_checks_all_files(self):
        all_files = [
            f.relative_to(_CHROMIUM_SRC_PATH)
            for f in (_CHROMIUM_SRC_PATH /
                      _TEST_FILES_PATH).resolve(strict=True).rglob('*.java')
            if f.name != 'InvalidSyntaxTest.java'
        ]

        test_health_infos = [
            f.test_dir / f.test_filename
            for f in test_health_extractor.get_repo_test_health(
                test_dir=_TEST_FILES_PATH,
                ignored_files={str(_INVALID_SYNTAX_TEST_PATH)})
        ]

        self.assertListEqual(all_files, test_health_infos)


if __name__ == '__main__':
    unittest.main()
