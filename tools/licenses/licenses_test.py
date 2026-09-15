#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..'))
sys.path.append(os.path.join(REPOSITORY_ROOT, 'tools', 'licenses'))

import licenses

class GetLicenseUrlsTest(unittest.TestCase):
    def setUp(self):
        super().setUp()
        self.temp_dir = tempfile.mkdtemp()
        # Clear caches before each test
        licenses._GIT_ORIGIN_CACHE.clear()
        licenses._GIT_ROOT_CACHE.clear()

    def tearDown(self):
        shutil.rmtree(self.temp_dir)
        super().tearDown()

    def _init_git_repo(self, repo_path):
        subprocess.check_call(['git', 'init'], cwd=repo_path)
        subprocess.check_call(['git', 'config', 'user.email', 'test@example.com'], cwd=repo_path)
        subprocess.check_call(['git', 'config', 'user.name', 'Test User'], cwd=repo_path)

    def _add_remote(self, repo_path, name, url):
        subprocess.check_call(['git', 'remote', 'add', name, url], cwd=repo_path)

    def test_standard_git_repo(self):
        repo_path = os.path.join(self.temp_dir, 'repo')
        os.makedirs(repo_path)
        self._init_git_repo(repo_path)
        self._add_remote(repo_path, 'origin', 'https://github.com/example/repo.git')

        # Create a dummy license file
        license_file = 'LICENSE'
        with open(os.path.join(repo_path, license_file), 'w') as f:
            f.write('Dummy license')

        # Run _GetLicenseUrls
        # scan_root is temp_dir
        # license_file is 'repo/LICENSE'
        urls = licenses._GetLicenseUrls(self.temp_dir, ['repo/LICENSE'])

        # git_root is temp_dir/repo
        # abs_f is temp_dir/repo/LICENSE
        # repo_rel is LICENSE
        expected_url = 'https://github.com/example/repo/+/main/LICENSE'
        self.assertEqual(urls, [expected_url])

    def test_multiple_remotes(self):
        repo_path = os.path.join(self.temp_dir, 'repo')
        os.makedirs(repo_path)
        self._init_git_repo(repo_path)
        self._add_remote(repo_path, 'upstream', 'https://github.com/upstream/repo.git')
        self._add_remote(repo_path, 'origin', 'https://github.com/example/repo.git')

        license_file = 'LICENSE'
        with open(os.path.join(repo_path, license_file), 'w') as f:
            f.write('Dummy license')

        urls = licenses._GetLicenseUrls(self.temp_dir, ['repo/LICENSE'])

        # Should prioritize origin
        expected_url = 'https://github.com/example/repo/+/main/LICENSE'
        self.assertEqual(urls, [expected_url])

    def test_non_git_directory(self):
        non_repo_path = os.path.join(self.temp_dir, 'non_repo')
        os.makedirs(non_repo_path)

        license_file = 'LICENSE'
        with open(os.path.join(non_repo_path, license_file), 'w') as f:
            f.write('Dummy license')

        urls = licenses._GetLicenseUrls(self.temp_dir, ['non_repo/LICENSE'])

        # Should fallback to Chromium URL
        expected_url = 'https://chromium.googlesource.com/chromium/src/+/main/non_repo/LICENSE'
        self.assertEqual(urls, [expected_url])

    def test_relative_scan_root(self):
        repo_path = os.path.join(self.temp_dir, 'repo')
        os.makedirs(repo_path)
        self._init_git_repo(repo_path)
        self._add_remote(repo_path, 'origin', 'https://github.com/example/repo.git')

        license_file = 'LICENSE'
        with open(os.path.join(repo_path, license_file), 'w') as f:
            f.write('Dummy license')

        # Use relative path if possible, but temp_dir is absolute.
        # Let's try to use os.path.relpath to test relative behavior.
        rel_scan_root = os.path.relpath(self.temp_dir, start=os.getcwd())

        urls = licenses._GetLicenseUrls(rel_scan_root, ['repo/LICENSE'])

        expected_url = 'https://github.com/example/repo/+/main/LICENSE'
        self.assertEqual(urls, [expected_url])

    def test_relative_local_remote(self):
        # Create an upstream repo
        upstream_path = os.path.join(self.temp_dir, 'upstream_repo')
        os.makedirs(upstream_path)
        self._init_git_repo(upstream_path)
        self._add_remote(upstream_path, 'origin', 'https://github.com/upstream/project.git')

        # Create a local clone directory that points to upstream via relative path
        repo_path = os.path.join(self.temp_dir, 'local_repo')
        os.makedirs(repo_path)
        self._init_git_repo(repo_path)
        # Add relative path remote '../upstream_repo'
        self._add_remote(repo_path, 'origin', '../upstream_repo')

        license_file = 'LICENSE'
        with open(os.path.join(repo_path, license_file), 'w') as f:
            f.write('Dummy license')

        urls = licenses._GetLicenseUrls(self.temp_dir, ['local_repo/LICENSE'])

        # Should recursively resolve through the relative local path to the upstream URL
        expected_url = 'https://github.com/upstream/project/+/main/LICENSE'
        self.assertEqual(urls, [expected_url])

    def test_absolute_scan_root(self):
        repo_path = os.path.join(self.temp_dir, 'repo')
        os.makedirs(repo_path)
        self._init_git_repo(repo_path)
        self._add_remote(repo_path, 'origin', 'https://github.com/example/repo.git')

        license_file = 'LICENSE'
        with open(os.path.join(repo_path, license_file), 'w') as f:
            f.write('Dummy license')

        urls = licenses._GetLicenseUrls(self.temp_dir, ['repo/LICENSE'])

        expected_url = 'https://github.com/example/repo/+/main/LICENSE'
        self.assertEqual(urls, [expected_url])
    def test_format_gitiles_url(self):
        base_url = 'https://chromium.googlesource.com/chromium/src'
        rel_path = 'third_party/foo/LICENSE'
        url = licenses._FormatGitilesUrl(base_url, rel_path)
        self.assertEqual(url, 'https://chromium.googlesource.com/chromium/src/+/main/third_party/foo/LICENSE')

        # Test path with Windows backslashes or Path object
        win_path = pathlib.PureWindowsPath('third_party\\foo\\LICENSE')
        url_from_win = licenses._FormatGitilesUrl(base_url, win_path)
        self.assertEqual(url_from_win, 'https://chromium.googlesource.com/chromium/src/+/main/third_party/foo/LICENSE')

    def test_resolve_remote_url(self):
        # Web URL ending in .git
        self.assertEqual(
            licenses._ResolveRemoteUrl('https://github.com/foo/bar.git', self.temp_dir),
            ['https://github.com/foo/bar']
        )
        # Web URL not ending in .git
        self.assertEqual(
            licenses._ResolveRemoteUrl('https://chromium.googlesource.com/foo/bar', self.temp_dir),
            ['https://chromium.googlesource.com/foo/bar']
        )

    def test_resolve_git_license_url_outside_repo(self):
        # Non-repo file should return None from _ResolveGitLicenseUrl
        non_repo_file = os.path.join(self.temp_dir, 'outside_LICENSE')
        with open(non_repo_file, 'w') as f:
            f.write('Dummy')
        self.assertIsNone(licenses._ResolveGitLicenseUrl(non_repo_file))


if __name__ == '__main__':
    unittest.main()
