#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for git_metadata_utils."""

import pathlib
import sys
import unittest
from datetime import datetime, timezone

_TOOLS_ANDROID_PATH = pathlib.Path(__file__).resolve(strict=True).parents[1]
if str(_TOOLS_ANDROID_PATH) not in sys.path:
    sys.path.append(str(_TOOLS_ANDROID_PATH))
from python_utils import git_metadata_utils

_TEST_FILE_FOLDER = pathlib.Path(__file__).parent.resolve(strict=True)

_TEST_GIT_INTERNAL = _TEST_FILE_FOLDER.joinpath('.git').resolve()

_CHROMIUM_SRC_ROOT = _TEST_FILE_FOLDER.parents[2]

# Note: git_metadata_utils (and these unit tests) do not depend on V8
# specifically; any public Git repo would work here.
_V8_GIT_ROOT = _CHROMIUM_SRC_ROOT.joinpath('v8').resolve(strict=True)

_MISSING_FILE_FOLDER = _TEST_FILE_FOLDER.joinpath('missing',
                                                  'folder').resolve()

_SHA1_HASH_REGEX = r'[0-9a-f]{40}'


class TestChromiumSrcPath(unittest.TestCase):
    """Tests for the get_chromium_src_path function."""

    def test_get_chromium_src_path(self):
        chromium_src_path = git_metadata_utils.get_chromium_src_path()

        self.assertEqual(_CHROMIUM_SRC_ROOT, chromium_src_path)
        self.assertTrue(chromium_src_path.exists())
        self.assertTrue(chromium_src_path.is_dir())
        self.assertEqual('src', chromium_src_path.name)
        self.assertTrue(chromium_src_path.joinpath('.git').exists())
        self.assertTrue(chromium_src_path.joinpath('.git').is_dir())


class TestHeadCommitHash(unittest.TestCase):
    """Tests for the get_head_commit_hash function."""

    def test_get_head_commit_hash_chromium_repo(self):
        """Tests that get_head_commit_hash returns a SHA1 commit hash."""
        commit_hash = git_metadata_utils.get_head_commit_hash(
            git_repo=str(_CHROMIUM_SRC_ROOT))

        self.assertRegex(commit_hash, _SHA1_HASH_REGEX,
                         f'"{commit_hash}" is not a SHA1 hash.')

    def test_get_head_commit_hash_chromium_repo_path(self):
        """Tests that get_head_commit_hash handles pathlib Paths."""
        expected_commit_hash = git_metadata_utils.get_head_commit_hash(
            git_repo=str(_CHROMIUM_SRC_ROOT))

        commit_hash = git_metadata_utils.get_head_commit_hash(
            git_repo=_CHROMIUM_SRC_ROOT)

        self.assertEqual(expected_commit_hash, commit_hash)

    def test_get_head_commit_hash_default_repo(self):
        """Tests that the Chromium repo is used when no git_repo specified."""
        expected_commit_hash = git_metadata_utils.get_head_commit_hash(
            git_repo=_CHROMIUM_SRC_ROOT)

        commit_hash = git_metadata_utils.get_head_commit_hash()

        self.assertEqual(expected_commit_hash, commit_hash)

    def test_get_head_commit_hash_custom_repo(self):
        """Tests that the git_repo parameter controls the target repository."""
        chromium_commit_hash = git_metadata_utils.get_head_commit_hash()

        commit_hash = git_metadata_utils.get_head_commit_hash(
            git_repo=_V8_GIT_ROOT)

        self.assertRegex(commit_hash, _SHA1_HASH_REGEX,
                         f'"{commit_hash}" is not a SHA1 hash.')
        self.assertNotEqual(chromium_commit_hash, commit_hash)

    def test_get_head_commit_hash_repo_not_found(self):
        """Tests that ValueError is raised when git_repo is not a repo path."""
        with self.assertRaises(ValueError) as error_cm:
            git_metadata_utils.get_head_commit_hash(
                git_repo=_MISSING_FILE_FOLDER)

        self.assertEqual(
            f'The Git repository root "{_MISSING_FILE_FOLDER}" is'
            f' invalid; No such file or directory:'
            f' "{_MISSING_FILE_FOLDER.parent}".', error_cm.exception.args[0])

    def test_get_head_commit_hash_invalid_repo(self):
        """Tests that ValueError is raised when git_repo is not a repo path."""
        with self.assertRaises(ValueError) as error_cm:
            git_metadata_utils.get_head_commit_hash(git_repo=_TEST_FILE_FOLDER)

        self.assertEqual(
            f'The path "{_TEST_FILE_FOLDER}" is not a root directory for a Git'
            f' repository; No such file or directory: "{_TEST_GIT_INTERNAL}".',
            error_cm.exception.args[0])


class TestHeadCommitDatetime(unittest.TestCase):
    """Tests for the get_head_commit_datetime function."""

    def test_get_head_commit_datetime_chromium_repo(self):
        """Tests that get_head_commit_datetime returns a commit datetime.

        Checks that the datetime is not naive (has a timezone, specifically
        UTC) and that the datetime is sane.
        """
        commit_datetime = git_metadata_utils.get_head_commit_datetime(
            git_repo=_CHROMIUM_SRC_ROOT)

        self.assertIsNotNone(commit_datetime.tzinfo)
        self.assertIsNotNone(commit_datetime.utcoffset())
        self.assertEqual(timezone.utc, commit_datetime.tzinfo)
        self.assertGreater(commit_datetime,
                           datetime(2021, 10, 5, tzinfo=timezone.utc))
        self.assertLess(commit_datetime,
                        datetime(2050, 1, 1, tzinfo=timezone.utc))

    def test_get_head_commit_datetime_chromium_repo_path(self):
        """Tests that get_head_commit_datetime handles pathlib Paths."""
        expected_commit_datetime = git_metadata_utils.get_head_commit_datetime(
            git_repo=str(_CHROMIUM_SRC_ROOT))

        commit_datetime = git_metadata_utils.get_head_commit_datetime(
            git_repo=_CHROMIUM_SRC_ROOT)

        self.assertEqual(expected_commit_datetime, commit_datetime)

    def test_get_head_commit_datetime_default_repo(self):
        """Tests that the Chromium repo is used when no git_repo specified."""
        expected_commit_datetime = git_metadata_utils.get_head_commit_datetime(
            git_repo=_CHROMIUM_SRC_ROOT)

        commit_datetime = git_metadata_utils.get_head_commit_datetime()

        self.assertEqual(expected_commit_datetime, commit_datetime)

    def test_get_head_commit_datetime_custom_repo(self):
        """Tests that the git_repo parameter controls the target repository."""
        chromium_datetime = git_metadata_utils.get_head_commit_datetime()

        commit_datetime = git_metadata_utils.get_head_commit_datetime(
            git_repo=_V8_GIT_ROOT)

        self.assertNotEqual(chromium_datetime, commit_datetime)

    def test_get_head_commit_datetime_repo_not_found(self):
        """Tests that ValueError is raised when git_repo is not a repo path."""
        with self.assertRaises(ValueError) as error_cm:
            git_metadata_utils.get_head_commit_datetime(
                git_repo=_MISSING_FILE_FOLDER)

        self.assertEqual(
            f'The Git repository root "{_MISSING_FILE_FOLDER}" is'
            f' invalid; No such file or directory:'
            f' "{_MISSING_FILE_FOLDER.parent}".', error_cm.exception.args[0])

    def test_get_head_commit_datetime_invalid_repo(self):
        """Tests that ValueError is raised when git_repo is not a repo path."""
        with self.assertRaises(ValueError) as error_cm:
            git_metadata_utils.get_head_commit_datetime(
                git_repo=_TEST_FILE_FOLDER)

        self.assertEqual(
            f'The path "{_TEST_FILE_FOLDER}" is not a root directory for a Git'
            f' repository; No such file or directory: "{_TEST_GIT_INTERNAL}".',
            error_cm.exception.args[0])


if __name__ == '__main__':
    unittest.main()
