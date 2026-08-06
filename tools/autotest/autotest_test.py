#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import unittest
from unittest import mock

from click.testing import CliRunner
import main

import finders.file_finder as file_finder
import finders.target_finder as target_finder
import test_executor
import utils.constants as const
from utils.command_error import AutotestError, CommandError

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

  def create_rs_test(self, path):
    self.fs.create_file(path, contents='#[gtest(A, B)]')

  def test_rs_test(self):
    self.create_rs_test('foo_unittest.rs')
    self.assertEqual(['foo_unittest.rs'],
                     file_finder.FindMatchingTestFiles('foo_unittest.rs'))

  def test_mm_test(self):
    self.create_cc_test('foo_unittest.mm')
    self.assertEqual(['foo_unittest.mm'],
                     file_finder.FindMatchingTestFiles('foo_unittest.mm'))

  def test_cc_alt_test(self):
    self.fs.create_file('foo.cc')
    self.create_cc_test('foo_unittest.cc')
    self.assertEqual(['foo_unittest.cc'],
                     file_finder.FindMatchingTestFiles('foo.cc'))

  def test_rs_alt_test(self):
    self.fs.create_file('foo.rs')
    self.create_rs_test('foo_unittest.rs')
    self.assertEqual(['foo_unittest.rs'],
                     file_finder.FindMatchingTestFiles('foo.rs'))

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

  def test_web_test_html(self):
    path = os.path.join(const.SRC_DIR, 'third_party', 'blink', 'web_tests',
                        'fast', 'media', 'mq-display-mode.html')
    self.fs.create_file(path)
    self.assertEqual([path], file_finder.FindMatchingTestFiles(path))

  def test_web_test_js(self):
    path = os.path.join(const.SRC_DIR, 'third_party', 'blink', 'web_tests',
                        'fast', 'media', 'mq-display-mode.js')
    self.fs.create_file(path)
    self.assertEqual([path], file_finder.FindMatchingTestFiles(path))

  def test_web_test_directory(self):
    test_dir = os.path.join(const.SRC_DIR, 'third_party', 'blink', 'web_tests',
                            'fast', 'media')
    test_file = os.path.join(test_dir, 'mq-display-mode.html')
    self.fs.create_dir(test_dir)
    self.fs.create_file(test_file)
    self.assertEqual([test_file], file_finder.FindMatchingTestFiles(test_dir))

  def test_web_test_exclude_resources(self):
    test_dir = os.path.join(const.SRC_DIR, 'third_party', 'blink', 'web_tests',
                            'fast', 'media')
    resources_dir = os.path.join(test_dir, 'resources')
    test_file = os.path.join(test_dir, 'mq-display-mode.html')
    resource_file = os.path.join(resources_dir, 'helper.html')

    self.fs.create_dir(resources_dir)
    self.fs.create_file(test_file)
    self.fs.create_file(resource_file)

    self.assertEqual([test_file], file_finder.FindMatchingTestFiles(test_dir))

  def test_web_test_exclude_expectations(self):
    test_dir = os.path.join(const.SRC_DIR, 'third_party', 'blink', 'web_tests',
                            'fast', 'media')
    test_file = os.path.join(test_dir, 'mq-display-mode.html')
    expected_file = os.path.join(test_dir, 'mq-display-mode-expected.html')

    self.fs.create_dir(test_dir)
    self.fs.create_file(test_file)
    self.fs.create_file(expected_file)

    self.assertEqual([test_file], file_finder.FindMatchingTestFiles(test_dir))

  def test_webui_tests(self):
    # Setup a fake WebUI directory structure under const.SRC_DIR
    webui_dir = os.path.join(const.SRC_DIR, 'chrome', 'test', 'data', 'webui',
                             'glic')
    unit_tests_dir = os.path.join(webui_dir, 'unit_tests')
    self.fs.create_dir(unit_tests_dir)

    # Create the C++ wrapper containing references to the JS files
    cc_wrapper = os.path.join(webui_dir, 'glic_browsertest.cc')
    self.fs.create_file(cc_wrapper,
                        contents='TEST_F(GlicWebUIBrowserTest, All) {\n'
                        '  RunTest("glic/unit_tests/glic_api_host_test.js")\n'
                        '  RunTest("glic/unit_tests/glic_api_client_test.js")\n'
                        '}')

    # Create the TS test files
    ts_file1 = os.path.join(unit_tests_dir, 'glic_api_host_test.ts')
    ts_file2 = os.path.join(unit_tests_dir, 'glic_api_client_test.ts')
    ts_file_ignored = os.path.join(unit_tests_dir, 'glic_api_ignored_test.ts')

    self.fs.create_file(ts_file1, contents='// TS test 1')
    self.fs.create_file(ts_file2, contents='// TS test 2')
    self.fs.create_file(ts_file_ignored, contents='// TS test ignored')

    # Assertion 1: Running on a single TS file that is referenced should
    # return the C++ wrapper
    self.assertEqual([cc_wrapper], file_finder.FindMatchingTestFiles(ts_file1))

    # Assertion 2: Running on a single TS file that is NOT referenced should
    # raise an AutotestError
    with self.assertRaises(AutotestError) as cm:
      file_finder.FindMatchingTestFiles(ts_file_ignored)
    self.assertIn("doesn't look like a test file", str(cm.exception))

    # Assertion 3: Searching the unit_tests directory should return only
    # the C++ wrapper (deduplicated)
    self.assertEqual([cc_wrapper],
                     file_finder.FindMatchingTestFiles(unit_tests_dir))

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
         mock.patch(
             'finders.file_finder._RecursiveMatchFilename') as mock_recursive:

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
    self.mock_run_command.runnable_targets = []
    self.mock_run_command.non_runnable_targets = []

    def side_effect(cmd, *args, **kwargs):
      runnable = '\n'.join(self.mock_run_command.runnable_targets)
      non_runnable = '\n'.join(self.mock_run_command.non_runnable_targets)
      if any(
          c.startswith('--exclude-type=source_set,static_library,rust_library')
          for c in cmd):
        return runnable
      return runnable + ('\n' + non_runnable if non_runnable else '')

    self.mock_run_command.side_effect = side_effect

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
    self.mock_run_command.runnable_targets = [
        "//:blink_tests", "//:gn_all", "//chrome/test:browser_tests",
        "//chrome/test:performance_browser_tests",
        "//third_party/blink/public:all_blink",
        "//third_party/blink/renderer/platform/wtf:wtf_unittests",
        "//third_party/blink/renderer/platform/wtf:wtf_unittests_sources"
    ]
    targets, _ = target_finder.FindTestTargets(self.mock_cache,
                                               self.out_dir, ['foo.cc'],
                                               run_all=True)

    self.assertIn('chrome/test:browser_tests', targets)
    self.assertIn('third_party/blink/renderer/platform/wtf:wtf_unittests',
                  targets)
    self.assertNotIn('//:blink_tests', targets)

  def test_internal_suffixes(self):
    self.mock_run_command.runnable_targets = [
        "//chrome/android:chrome_public_test_apk__java_binary",
        "//chrome/android:chrome_public_test_apk__test_apk"
    ]
    targets, _ = target_finder.FindTestTargets(self.mock_cache, self.out_dir,
                                               ['foo.java'])
    # Should strip suffix
    self.assertIn('chrome/android:chrome_public_test_apk', targets)

  def test_filter_non_runnable_targets(self):
    self.mock_run_command.runnable_targets = ["//chrome/test:browser_tests"]
    self.mock_run_command.non_runnable_targets = [
        "//components/payments:passkey_browser_binder_unittests"
    ]

    targets, _ = target_finder.FindTestTargets(self.mock_cache, self.out_dir,
                                               ['foo.cc'])

    self.assertEqual(['chrome/test:browser_tests'], targets)

  def test_allowlist(self):
    self.mock_run_command.runnable_targets = ["//chrome/test:browser_tests"]
    targets, _ = target_finder.FindTestTargets(self.mock_cache, self.out_dir,
                                               ['foo.cc'])
    self.assertIn('chrome/test:browser_tests', targets)

  def test_target_ambiguity_prompt(self):
    self.mock_run_command.runnable_targets = [
        "//chrome/test:unit_tests", "//chrome/test:browser_tests"
    ]
    with mock.patch('utils.command_util.HaveUserPickTarget',
                    return_value='//chrome/test:unit_tests') as mock_pick:
      targets, _ = target_finder.FindTestTargets(self.mock_cache, self.out_dir,
                                                 ['foo.cc'])
      self.assertEqual(['chrome/test:unit_tests'], targets)
      mock_pick.assert_called_once()
      self.assertEqual(mock_pick.call_args[0][0], None)

  def test_target_ambiguity_prompt_gemini_cli(self):
    self.mock_run_command.runnable_targets = [
        "//chrome/test:unit_tests", "//chrome/test:browser_tests"
    ]
    with mock.patch('utils.IsGeminiCli', return_value=True) as mock_pick:
      orig_paths = ['foo.cc']
      with self.assertRaises(SystemExit):
        target_finder.FindTestTargets(self.mock_cache,
                                      self.out_dir, ['foo.cc'],
                                      orig_paths=orig_paths)

  def test_target_index(self):
    self.mock_run_command.runnable_targets = [
        "//chrome/test:unit_tests", "//chrome/test:browser_tests"
    ]
    # Sorted: browser_tests, unit_tests. Index 0 -> browser_tests
    targets, _ = target_finder.FindTestTargets(self.mock_cache,
                                               self.out_dir, ['foo.cc'],
                                               target_index=0)
    self.assertEqual(['chrome/test:browser_tests'], targets)

  def test_run_all(self):
    self.mock_run_command.runnable_targets = [
        "//chrome/test:unit_tests", "//chrome/test:browser_tests"
    ]
    targets, _ = target_finder.FindTestTargets(self.mock_cache,
                                               self.out_dir, ['foo.cc'],
                                               run_all=True)
    self.assertEqual(len(targets), 2)
    self.assertIn('chrome/test:browser_tests', targets)
    self.assertIn('chrome/test:unit_tests', targets)


class FindRelatedTestFilesTest(TestCase):

  def setUp(self):
    super().setUp()
    self.setUpPyfakefs()
    self.fs.create_dir(const.SRC_DIR)

    # Mock RunCommand to simulate ripgrep calls
    self.mock_run_command = mock.patch('utils.command_util.RunCommand').start()
    self.addCleanup(mock.patch.stopall)

  def _create_command_error(self):
    """Helper to safely throw a CommandError with a return_code property."""
    # Pass the message and the return_code directly into the constructor
    return CommandError('rg no match', 1)

  def test_cxx_exact_match(self):
    self.fs.create_file('foo_unittest.cc', contents='TEST(A, B) {}')
    self.mock_run_command.return_value = 'foo_unittest.cc'

    results = file_finder._FindRelatedTestFiles('foo.cc')
    self.assertEqual(['foo_unittest.cc'], results)

  def test_cxx_modifier_applied(self):
    # Case: foo_bar_browsertest.cc belongs to foo.cc
    test_file = 'foo_bar_browsertest.cc'
    self.fs.create_file(test_file, contents='TEST(A, B) {}')

    def rg_mock(cmd):
      cmd_str = ' '.join(cmd)
      if 'foo*' in cmd_str:
        return test_file
      # Simulate existence check failing for intermediate stem
      # (foo_bar).
      if ('foo_bar.cc' in cmd_str or 'foo_bar.h' in cmd_str):
        raise self._create_command_error()
      return ''

    self.mock_run_command.side_effect = rg_mock
    results = file_finder._FindRelatedTestFiles('foo.cc')
    self.assertEqual([test_file], results)

  def test_cxx_different_file(self):
    # Case: foo_bar_unittest.cc belongs to foo_bar.cc,
    # so it should NOT be returned when foo.cc is modified.
    test_file = 'foo_bar_unittest.cc'
    self.fs.create_file(test_file, contents='TEST(A, B) {}')

    def rg_mock(cmd):
      cmd_str = ' '.join(cmd)
      if 'foo*' in cmd_str:
        return test_file
      # Simulate finding the intermediate stem file
      if ('foo_bar.cc' in cmd_str or 'foo_bar.h' in cmd_str):
        return 'foo_bar.cc'
      raise self._create_command_error()

    self.mock_run_command.side_effect = rg_mock
    results = file_finder._FindRelatedTestFiles('foo.cc')
    self.assertEqual([], results)

  def test_rs_exact_match(self):
    self.fs.create_file('foo_unittest.rs', contents='#[gtest(A, B)]')
    self.mock_run_command.return_value = 'foo_unittest.rs'

    results = file_finder._FindRelatedTestFiles('foo.rs')
    self.assertEqual(['foo_unittest.rs'], results)

  def test_java_exact_match(self):
    self.fs.create_file('FooTest.java', contents='@Test')
    self.mock_run_command.return_value = 'FooTest.java'

    results = file_finder._FindRelatedTestFiles('Foo.java')
    self.assertEqual(['FooTest.java'], results)

  def test_java_modifier_applied(self):
    # Case: FooBarTest.java belongs to Foo.java
    test_file = 'FooBarTest.java'
    self.fs.create_file(test_file, contents='@Test')

    def rg_mock(cmd):
      cmd_str = ' '.join(cmd)
      if 'Foo*' in cmd_str:
        return test_file
      # FooBar.java does NOT exist
      if 'FooBar.java' in cmd_str:
        raise self._create_command_error()
      return ''

    self.mock_run_command.side_effect = rg_mock
    results = file_finder._FindRelatedTestFiles('Foo.java')
    self.assertEqual([test_file], results)

  def test_java_different_file(self):
    # Case: FooBarTest.java belongs to FooBar.java
    test_file = 'FooBarTest.java'
    self.fs.create_file(test_file, contents='@Test')

    def rg_mock(cmd):
      cmd_str = ' '.join(cmd)
      if 'Foo*' in cmd_str:
        return test_file
      # FooBar.java DOES exist
      if 'FooBar.java' in cmd_str:
        return 'FooBar.java'
      raise self._create_command_error()

    self.mock_run_command.side_effect = rg_mock
    results = file_finder._FindRelatedTestFiles('Foo.java')
    self.assertEqual([], results)

  def test_java_different_file_modifier_applied(self):
    # Case: FooBarIntegrationTest.java belongs to
    # FooBar.java. It has a modifier (Integration),
    # but it still belongs to the Bar, NOT Foo.java.
    # Therefore, it should NOT be returned when Foo.java
    # is modified.
    test_file = 'FooBarIntegrationTest.java'
    self.fs.create_file(test_file, contents='@Test')

    def rg_mock(cmd):
      cmd_str = ' '.join(cmd)
      # 1. Candidate search finds our test file.
      if 'Foo*' in cmd_str:
        return test_file
      # 2. First peel: FooBarIntegration.java does NOT exist.
      if 'FooBarIntegration.java' in cmd_str:
        raise self._create_command_error()
      # 3. Second peel: FooBar.java DOES exist.
      if 'FooBar.java' in cmd_str:
        return 'FooBar.java'
      raise self._create_command_error()

    self.mock_run_command.side_effect = rg_mock
    results = file_finder._FindRelatedTestFiles('Foo.java')

    # Assert that the test is correctly discarded!
    self.assertEqual([], results)

  def test_unsupported_extension(self):
    # Non-supported files should return empty immediately without
    # querying rg.
    results = file_finder._FindRelatedTestFiles('README.md')
    self.assertEqual([], results)
    self.mock_run_command.assert_not_called()

  @mock.patch('shutil.which', return_value='/usr/bin/csearch')
  @mock.patch('finders.file_finder._CodeSearchFiles')
  def test_remote_search(self, mock_cs, mock_which):
    test_file = 'foo_bar_browsertest.cc'
    self.fs.create_file(test_file, contents='TEST(A, B) {}')

    def cs_mock(cmd):
      cmd_str = ' '.join(cmd)
      # Candidate search
      if 'foo[^/]*\\.' in cmd_str:
        return [test_file]
      # Existence check for intermediate stem.
      if 'foo_bar\\.' in cmd_str:
        return []
      return []

    mock_cs.side_effect = cs_mock

    results = file_finder._FindRelatedTestFiles('foo.cc', remote_search=True)
    self.assertEqual([test_file], results)
    self.mock_run_command.assert_not_called()


class SearchForTestsByNameTest(TestCase):

  def setUp(self):
    super().setUp()
    self.setUpPyfakefs()
    self.mock_run_command = mock.patch('utils.command_util.RunCommand').start()
    self.addCleanup(mock.patch.stopall)

  def test_class_method_syntax(self):
    test_file = 'FooTest.java'
    self.fs.create_file(test_file,
                        contents='class FooTest { @Test void foo() {} }')

    # Mock RunCommand for ripgrep to return the file
    self.mock_run_command.return_value = test_file

    files, filter = file_finder.SearchForTestsByName(['FooTest#testMethod'],
                                                     quiet=True,
                                                     remote_search=False)

    self.assertEqual([test_file], files)
    self.assertEqual('FooTest#testMethod', filter)

    called_args = self.mock_run_command.call_args[0][0]
    self.assertIn('(\\bFooTest\\b)', called_args)


# Tests execution of multiple test targets to ensure correct flag isolation.
class RunTestTargetsTest(TestCase):

  def setUp(self):
    super().setUp()
    self.setUpPyfakefs()
    self.out_dir = os.path.join(const.SRC_DIR, 'out', 'Default')
    self.fs.create_dir(self.out_dir)

    # Mock RunTestCommandWithSummary to avoid running actual commands
    self.mock_run_with_summary = mock.patch(
        'test_executor.command.RunTestCommandWithSummary').start()
    from utils.command_util import TestSummary
    self.mock_run_with_summary.return_value = (0, TestSummary(test_count=1))
    self.addCleanup(mock.patch.stopall)

  def test_extra_args_pollution(self):
    # Setup: Create the Android wrapper script for base_unittests so it triggers
    # the addition of --fast-local-dev and --single-variant.
    wrapper_path = os.path.join(self.out_dir, 'bin', 'run_base_unittests')
    self.fs.create_file(wrapper_path)

    # We also need to simulate blink web tests runner path
    web_tests_runner = os.path.join(const.SRC_DIR, 'third_party', 'blink',
                                    'tools', 'run_web_tests.py')
    self.fs.create_file(web_tests_runner)

    targets = ['base:base_unittests', 'blink_tests']
    web_test_files = [
        'third_party/blink/web_tests/fast/media/mq-display-mode.html'
    ]

    # Run the executor
    test_executor.RunTestTargets(out_dir=self.out_dir,
                                 targets=targets,
                                 gtest_filter='ValuesTest.*',
                                 pref_mapping_filter=None,
                                 extra_args=['--some-shared-arg'],
                                 dry_run=False,
                                 no_try_android_wrappers=False,
                                 no_fast_local_dev=False,
                                 no_single_variant=False,
                                 web_test_files=web_test_files)

    # Verify calls to RunTestCommandWithSummary
    self.assertEqual(self.mock_run_with_summary.call_count, 2)

    # First call (GTest base_unittests)
    first_call_args = self.mock_run_with_summary.call_args_list[0][0][0]
    self.assertIn('--fast-local-dev', first_call_args)
    self.assertIn('--single-variant', first_call_args)
    self.assertIn('--some-shared-arg', first_call_args)

    # Second call (Web Test blink_tests)
    second_call_args = self.mock_run_with_summary.call_args_list[1][0][0]
    # It should contain the shared arg
    self.assertIn('--some-shared-arg', second_call_args)
    # BUT it should NOT contain the GTest-specific wrapper flags!
    self.assertNotIn('--fast-local-dev', second_call_args)
    self.assertNotIn('--single-variant', second_call_args)


class MainExitCodeTest(TestCase):

  def setUp(self):
    super().setUp()
    self.setUpPyfakefs()

    # Create SRC_DIR and test file in fake fs
    self.fs.create_dir(const.SRC_DIR)
    self.test_file = os.path.join(const.SRC_DIR, 'foo_unittest.cc')
    self.fs.create_file(self.test_file, contents='TEST(A, B) {}')

    self.out_dir = os.path.join(const.SRC_DIR, 'out', 'Default')
    self.fs.create_dir(self.out_dir)
    self.fs.create_file(os.path.join(self.out_dir, 'build.ninja'))

    # Mock RunCommand to simulate gn refs (matches style of FindTestTargetsTest)
    self.mock_run_command = mock.patch('utils.command_util.RunCommand').start()
    self.mock_run_command.runnable_targets = ["//chrome/test:unit_tests"]
    self.mock_run_command.non_runnable_targets = []

    def side_effect(cmd, *args, **kwargs):
      runnable = '\n'.join(self.mock_run_command.runnable_targets)
      non_runnable = '\n'.join(self.mock_run_command.non_runnable_targets)
      if any(
          c.startswith('--exclude-type=source_set,static_library,rust_library')
          for c in cmd):
        return runnable
      return runnable + ('\n' + non_runnable if non_runnable else '')

    self.mock_run_command.side_effect = side_effect

    # Mock build and run
    self.mock_build = mock.patch('main.test_executor.BuildTestTargets',
                                 return_value=True).start()
    self.mock_run = mock.patch('main.test_executor.RunTestTargets').start()

    # Mock filters to avoid subprocess + fakefs issues
    mock.patch('main.filters.BuildTestFilter',
               return_value='DummyFilter.*').start()
    mock.patch(
        'main.filters.BuildPrefMappingTestFilter',
        return_value='DummyPrefFilter.*',
    ).start()

    # Mock telemetry to avoid opentelemetry crash when not initialized
    mock.patch('main.telemetry.RecordMainAttributes').start()

    self.addCleanup(mock.patch.stopall)

  def test_main_success(self):
    self.mock_run.return_value = 0
    runner = CliRunner()
    result = runner.invoke(main.main, ['-C', self.out_dir, self.test_file])
    self.assertEqual(result.exit_code, 0)
    self.mock_run.assert_called_once()

  def test_main_test_failure(self):
    self.mock_run.return_value = 5
    runner = CliRunner()
    result = runner.invoke(main.main, ['-C', self.out_dir, self.test_file])
    self.assertEqual(result.exit_code, 5)
    self.mock_run.assert_called_once()

  def test_main_build_failure(self):
    self.mock_build.return_value = False
    runner = CliRunner()
    result = runner.invoke(main.main, ['-C', self.out_dir, self.test_file])
    self.assertEqual(result.exit_code, 1)
    self.mock_run.assert_not_called()


if __name__ == '__main__':
  unittest.main()
