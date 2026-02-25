#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import unittest
from unittest import mock

import finders.file_finder as file_finder
import finders.target_finder as target_finder
import utils.constants as const
from utils.command_error import AutotestError

from pyfakefs.fake_filesystem_unittest import TestCase

# Helper to create a dummy test file content
GTEST_CONTENT = 'TEST(A, B) {}'

class FindMatchingTestFilesTest(TestCase):

  def setUp(self):
    super().setUp()
    self.setUpPyfakefs()
    # Create SRC_DIR in fake fs to avoid issues with absolute paths
    self.fs.create_dir(const.SRC_DIR)

  def create_cc_test(self, path):
    self.fs.create_file(path, contents=GTEST_CONTENT)

  def test_cc_test(self):
    self.create_cc_test('foo_unittest.cc')
    self.assertEqual(['foo_unittest.cc'],
                     file_finder.FindMatchingTestFiles('foo_unittest.cc'))

  def test_mm_test(self):
    self.create_cc_test('foo_unittest.mm')
    self.assertEqual(['foo_unittest.mm'],
                     file_finder.FindMatchingTestFiles('foo_unittest.mm'))

  def test_cc_alt_test(self):
    self.fs.create_file('foo.cc')
    self.create_cc_test('foo_unittest.cc')
    self.assertEqual(['foo_unittest.cc'],
                     file_finder.FindMatchingTestFiles('foo.cc'))

  def test_cc_maybe_test(self):
    self.fs.create_file('foo_unittest.cc')
    self.assertEqual(['foo_unittest.cc'],
                     file_finder.FindMatchingTestFiles('foo_unittest.cc'))

  def test_cc_alt_maybe_test(self):
    self.fs.create_file('foo.cc')
    self.fs.create_file('foo_unittest.cc')
    self.assertEqual(['foo_unittest.cc'],
                     file_finder.FindMatchingTestFiles('foo.cc'))

  def test_cc_no_test(self):
    self.fs.create_file('foo.cc')
    with self.assertRaises(AutotestError) as cm:
      file_finder.FindMatchingTestFiles('foo.cc')
    self.assertEqual(str(cm.exception), "foo.cc doesn't look like a test file")

  def test_h_for_cc_test(self):
    self.fs.create_file('foo.h')
    self.create_cc_test('foo_unittest.cc')
    self.assertEqual(['foo_unittest.cc'],
                     file_finder.FindMatchingTestFiles('foo.h'))

  def test_h_for_mm_test(self):
    self.fs.create_file('foo.h')
    self.create_cc_test('foo_unittest.mm')
    self.assertEqual(['foo_unittest.mm'],
                     file_finder.FindMatchingTestFiles('foo.h'))

  def test_java(self):
    self.fs.create_file('Foo.java')
    self.assertEqual(['Foo.java'],
                     file_finder.FindMatchingTestFiles('Foo.java'))

  def test_directory_search(self):
    test_dir = os.path.join(const.SRC_DIR, 'foo_dir')
    test_file = os.path.join(test_dir, 'foo_test.cc')
    self.fs.create_dir(test_dir)
    self.create_cc_test(test_file)
    self.assertEqual([test_file], file_finder.FindMatchingTestFiles(test_dir))

  def test_recursive_search(self):
    # Setup: root/match.cc, root/subdir/match.cc
    root = os.path.join(const.SRC_DIR, 'search_root')
    subdir = os.path.join(root, 'subdir')
    self.fs.create_dir(subdir)

    file1 = os.path.join(root, 'match_test.cc')
    file2 = os.path.join(subdir, 'match_test.cc')
    self.create_cc_test(file1)
    self.create_cc_test(file2)

    with mock.patch('utils.command_util.HaveUserPickFile',
                    return_value=file1) as mock_pick:
      results = file_finder.FindMatchingTestFiles('match_test.cc')
      self.assertEqual([file1], results)
      mock_pick.assert_called_once()
      args, _ = mock_pick.call_args
      # We expect both files to be found
      found_files = set(args[0])
      self.assertIn(file1, found_files)
      self.assertIn(file2, found_files)

  def test_ignore_out_and_dot_dirs(self):
    # Setup: out/Default/foo_test.cc, .hidden/foo_test.cc, src/foo_test.cc
    out_dir = os.path.join(const.SRC_DIR, 'out', 'Default')
    hidden_dir = os.path.join(const.SRC_DIR, '.hidden')
    src_dir = os.path.join(const.SRC_DIR, 'src')

    self.fs.create_dir(out_dir)
    self.fs.create_dir(hidden_dir)
    self.fs.create_dir(src_dir)

    out_file = os.path.join(out_dir, 'foo_test.cc')
    hidden_file = os.path.join(hidden_dir, 'foo_test.cc')
    src_file = os.path.join(src_dir, 'foo_test.cc')

    self.create_cc_test(out_file)
    self.create_cc_test(hidden_file)
    self.create_cc_test(src_file)

    # Search for 'foo_test.cc'
    # Should only find src_file
    results = file_finder.FindMatchingTestFiles('foo_test.cc')
    self.assertEqual([src_file], results)

  def test_ambiguity_exact_over_partial(self):
    # Setup: foo_test.cc (exact), bar_foo_test.cc (partial)
    root = const.SRC_DIR
    exact_file = os.path.join(root, 'foo_test.cc')
    partial_file = os.path.join(root, 'bar_foo_test.cc')

    self.create_cc_test(exact_file)
    self.create_cc_test(partial_file)

    # Search for 'foo_test.cc'
    # Should prioritize exact match and NOT ask user
    with mock.patch('utils.command_util.HaveUserPickFile') as mock_pick:
      results = file_finder.FindMatchingTestFiles('foo_test.cc')
      self.assertEqual([exact_file], results)
      mock_pick.assert_not_called()

  def test_path_index_argument(self):
    # Setup: dir1/common_test.cc, dir2/common_test.cc
    dir1 = os.path.join(const.SRC_DIR, 'dir1')
    dir2 = os.path.join(const.SRC_DIR, 'dir2')
    self.fs.create_dir(dir1)
    self.fs.create_dir(dir2)

    file1 = os.path.join(dir1, 'common_test.cc')
    file2 = os.path.join(dir2, 'common_test.cc')
    self.create_cc_test(file1)
    self.create_cc_test(file2)

    with mock.patch('utils.command_util.HaveUserPickFile') as mock_pick:
      # path_index=0
      results = file_finder.FindMatchingTestFiles('common_test.cc',
                                                  path_index=0)
      self.assertEqual([file1], results)
      mock_pick.assert_not_called()

  @mock.patch('shutil.which', return_value='/usr/bin/csearch')
  @mock.patch('finders.file_finder._CodeSearchFiles')
  def test_remote_search_success(self, mock_cs, mock_which):
    # Setup remote results
    remote_file = os.path.join(const.SRC_DIR, 'remote_test.cc')
    self.create_cc_test(remote_file)
    mock_cs.return_value = [remote_file]

    results = file_finder.FindMatchingTestFiles('remote', remote_search=True)
    self.assertEqual([remote_file], results)
    mock_cs.assert_called()

  @mock.patch('shutil.which', return_value='/usr/bin/csearch')
  @mock.patch('finders.file_finder._CodeSearchFiles')
  def test_remote_search_fallback(self, mock_cs, mock_which):
    # Remote returns empty, fallback to local
    mock_cs.return_value = []

    local_file = os.path.join(const.SRC_DIR, 'local_test.cc')
    self.create_cc_test(local_file)

    results = file_finder.FindMatchingTestFiles('local_test.cc',
                                                remote_search=True)
    self.assertEqual([local_file], results)

  @mock.patch('sys.platform', 'win32')
  def test_windows_path_normalization(self):
    with mock.patch.object(file_finder.os.path, 'altsep', '/'), \
         mock.patch.object(file_finder.os.path, 'sep', '\\'), \
         mock.patch('finders.file_finder._RecursiveMatchFilename') as mock_recursive:

      mock_recursive.return_value = ([], [])
      with self.assertRaises(AutotestError):
        file_finder.FindMatchingTestFiles('dir/file.cc')

      # Verify target was normalized
      mock_recursive.assert_called()
      self.assertEqual(mock_recursive.call_args[0][1], 'dir\\file.cc')


class TargetCacheTest(TestCase):

  def setUp(self):
    super().setUp()
    self.setUpPyfakefs()
    self.out_dir = os.path.join(const.SRC_DIR, 'out', 'Default')
    self.fs.create_dir(self.out_dir)
    self.ninja_path = os.path.join(self.out_dir, 'build.ninja')
    self.fs.create_file(self.ninja_path)
    # Set mtime
    os.utime(self.ninja_path, (100, 100))

  def test_save_and_load(self):
    cache = target_finder.TargetCache(self.out_dir)
    test_files = ['/path/to/test.cc']
    targets = ['//target:test']

    cache.Store(test_files, targets)
    cache.Save()

    # Reload
    new_cache = target_finder.TargetCache(self.out_dir)
    self.assertEqual(new_cache.Find(test_files), targets)

  def test_invalidation_on_ninja_change(self):
    cache = target_finder.TargetCache(self.out_dir)
    test_files = ['/path/to/test.cc']
    targets = ['//target:test']
    cache.Store(test_files, targets)
    cache.Save()

    # Update ninja mtime
    os.utime(self.ninja_path, (200, 200))

    new_cache = target_finder.TargetCache(self.out_dir)
    self.assertIsNone(new_cache.Find(test_files))


class FindTestTargetsTest(TestCase):

  def setUp(self):
    super().setUp()
    self.setUpPyfakefs()
    # Mock command_util.RunCommand to simulate gn refs
    self.mock_run_command = mock.patch('utils.command_util.RunCommand').start()
    self.mock_exit = mock.patch('utils.command_util.ExitWithMessage').start()
    self.mock_exit.side_effect = Exception("ExitWithMessage called")
    self.addCleanup(mock.patch.stopall)

    self.out_dir = 'out/Default'
    self.fs.create_dir(self.out_dir)
    self.fs.create_file(os.path.join(self.out_dir, 'build.ninja'))

    self.mock_cache = mock.Mock(spec=target_finder.TargetCache)
    self.mock_cache.Find.return_value = None
    self.mock_cache.GetBuildNinjaMtime.return_value = 100

  def test_mixed_targets(self):
    # Simulate `gn refs` output for the command:
    # $ gn refs out_/Default --all --relation=source --relation=input \
    #     chrome/browser/ui/browser_browsertest.cc \
    #     third_party/blink/renderer/platform/wtf/vector_test.cc
    self.mock_run_command.return_value = """
//:blink_tests
//:gn_all
//chrome/test:browser_tests
//chrome/test:performance_browser_tests
//third_party/blink/public:all_blink
//third_party/blink/renderer/platform/wtf:wtf_unittests
//third_party/blink/renderer/platform/wtf:wtf_unittests_sources
"""
    targets, _ = target_finder.FindTestTargets(self.mock_cache,
                                               self.out_dir, ['foo.cc'],
                                               run_all=True)

    self.assertIn('chrome/test:browser_tests', targets)
    self.assertIn('third_party/blink/renderer/platform/wtf:wtf_unittests',
                  targets)
    self.assertNotIn('//:blink_tests', targets)

  def test_internal_suffixes(self):
    self.mock_run_command.return_value = """
//chrome/android:chrome_public_test_apk__java_binary
//chrome/android:chrome_public_test_apk__test_apk
"""
    targets, _ = target_finder.FindTestTargets(self.mock_cache, self.out_dir,
                                               ['foo.java'])
    # Should strip suffix
    self.assertIn('chrome/android:chrome_public_test_apk', targets)

  def test_allowlist(self):
    self.mock_run_command.return_value = """
//chrome/test:browser_tests
"""
    targets, _ = target_finder.FindTestTargets(self.mock_cache, self.out_dir,
                                               ['foo.cc'])
    self.assertIn('chrome/test:browser_tests', targets)

  def test_target_ambiguity_prompt(self):
    self.mock_run_command.return_value = """
//chrome/test:unit_tests
//chrome/test:browser_tests
"""
    with mock.patch('utils.command_util.HaveUserPickTarget',
                    return_value='//chrome/test:unit_tests') as mock_pick:
      targets, _ = target_finder.FindTestTargets(self.mock_cache, self.out_dir,
                                                 ['foo.cc'])
      self.assertEqual(['chrome/test:unit_tests'], targets)
      mock_pick.assert_called_once()

  def test_target_index(self):
    self.mock_run_command.return_value = """
//chrome/test:unit_tests
//chrome/test:browser_tests
"""
    # Sorted: browser_tests, unit_tests. Index 0 -> browser_tests
    targets, _ = target_finder.FindTestTargets(self.mock_cache,
                                               self.out_dir, ['foo.cc'],
                                               target_index=0)
    self.assertEqual(['chrome/test:browser_tests'], targets)

  def test_run_all(self):
    self.mock_run_command.return_value = """
//chrome/test:unit_tests
//chrome/test:browser_tests
"""
    targets, _ = target_finder.FindTestTargets(self.mock_cache,
                                               self.out_dir, ['foo.cc'],
                                               run_all=True)
    self.assertEqual(len(targets), 2)
    self.assertIn('chrome/test:browser_tests', targets)
    self.assertIn('chrome/test:unit_tests', targets)

if __name__ == '__main__':
  unittest.main()
