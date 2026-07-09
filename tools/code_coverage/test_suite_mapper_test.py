#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for test_suite_mapper.py."""

import contextlib
import io
import pathlib
import unittest
from unittest.mock import MagicMock, patch

from pyfakefs import fake_filesystem_unittest

import test_suite_mapper

MOCK_ISOLATE_MAP = {
    '//chrome/test:browser_tests': 'browser_tests',
    '//chrome/test:unit_tests': 'unit_tests',
    '//media/midi:midi_unittests': 'midi_unittests',
    '//media:media_unittests': 'media_unittests',
    '//chrome/browser:foo_tests': 'foo_tests',
    '//content/test:bar_tests': 'bar_tests',
    '//foo/bar:baz_tests': 'baz_tests',
    '//base:base_unittests': 'base_unittests',
    '//:blink_wpt_tests': 'blink_wpt_tests',
}


class TestSuiteMapperUnit(fake_filesystem_unittest.TestCase):

  def setUp(self):
    self.setUpPyfakefs()
    self.subprocess_patcher = patch('subprocess.run')
    self.mock_run = self.subprocess_patcher.start()
    self.addCleanup(self.subprocess_patcher.stop)

  def test_common_dir_prefix_len(self):
    self.assertEqual(test_suite_mapper.common_dir_prefix_len('a/b/c', 'a/b/c'),
                     3)
    self.assertEqual(test_suite_mapper.common_dir_prefix_len('a/b/c', 'a/b/d'),
                     2)
    self.assertEqual(test_suite_mapper.common_dir_prefix_len('a/b/c', 'x/y/z'),
                     0)
    self.assertEqual(test_suite_mapper.common_dir_prefix_len('a/b/c', ''), 0)
    self.assertEqual(test_suite_mapper.common_dir_prefix_len('a/b', 'a/b/c'), 2)

  def test_parse_isolate_map_valid(self):
    content = ("{'browser_tests': {'label': '//chrome/test:browser_tests'},"
               " 'unit_tests': {'label': '//chrome/test:unit_tests'}}")
    expected = {
        '//chrome/test:browser_tests': 'browser_tests',
        '//chrome/test:unit_tests': 'unit_tests',
    }
    self.assertEqual(test_suite_mapper.parse_isolate_map(content, 'test'),
                     expected)

  def test_parse_isolate_map_missing_label(self):
    content = "{'suite1': {'type': 'additional_compile_target'}}"
    self.assertEqual(test_suite_mapper.parse_isolate_map(content, 'test'), {})

  def test_parse_isolate_map_invalid(self):
    content = 'invalid dict'
    f = io.StringIO()
    with contextlib.redirect_stderr(f):
      self.assertIsNone(test_suite_mapper.parse_isolate_map(content, 'test'))
    self.assertTrue(len(f.getvalue()) > 0)

  def test_parse_isolate_map_not_dict(self):
    content = '[1, 2, 3]'
    f = io.StringIO()
    with contextlib.redirect_stderr(f):
      self.assertIsNone(test_suite_mapper.parse_isolate_map(content, 'test'))
    self.assertIn('Expected a dictionary', f.getvalue())

  def test_load_isolate_map_success(self):
    self.fs.create_dir('/src')
    pyl_path = pathlib.Path('/src') / test_suite_mapper.ISOLATE_MAP_PATH
    self.fs.create_file(
        pyl_path,
        contents="{'browser_tests': {'label': '//chrome/test:browser_tests'}}",
    )
    isolate_map = test_suite_mapper.load_isolate_map('/src')
    self.assertEqual(isolate_map,
                     {'//chrome/test:browser_tests': 'browser_tests'})

  def test_load_isolate_map_missing_file(self):
    self.fs.create_dir('/src')
    f = io.StringIO()
    with contextlib.redirect_stderr(f):
      self.assertIsNone(test_suite_mapper.load_isolate_map('/src'))
    self.assertIn('Isolate map not found', f.getvalue())

  def test_load_isolate_map_read_error(self):
    self.fs.create_dir('/src')
    pyl_path = pathlib.Path('/src') / test_suite_mapper.ISOLATE_MAP_PATH
    self.fs.create_dir(pyl_path)
    f = io.StringIO()
    with contextlib.redirect_stderr(f):
      self.assertIsNone(test_suite_mapper.load_isolate_map('/src'))
    self.assertIn('Error reading', f.getvalue())

  def test_extract_test_suites_basic(self):
    refs = [
        '//chrome/browser:foo_tests',
        '//content/test:bar_tests',
        '//foo/bar:baz_tests',
    ]
    suites = test_suite_mapper.extract_test_suites(
        refs, pathlib.Path('chrome/browser/foo.cc'), MOCK_ISOLATE_MAP)
    self.assertEqual(suites, ['foo_tests'])

  def test_extract_test_suites_proximity(self):
    refs = [
        '//chrome/test:browser_tests',
        '//media/midi:midi_unittests',
    ]
    suites = test_suite_mapper.extract_test_suites(
        refs, pathlib.Path('media/midi/midi_manager.cc'), MOCK_ISOLATE_MAP)
    self.assertEqual(suites, ['midi_unittests'])

  def test_extract_test_suites_filter_non_isolate(self):
    refs = [
        '//media/base:unit_tests',
        '//media:media_unittests',
    ]
    suites = test_suite_mapper.extract_test_suites(
        refs,
        pathlib.Path('media/base/audio_bus_unittest.cc'),
        MOCK_ISOLATE_MAP,
    )
    self.assertEqual(suites, ['media_unittests'])

  def test_extract_test_suites_empty_target(self):
    refs = ['//chrome/browser:foo_tests', '', '  ']
    suites = test_suite_mapper.extract_test_suites(
        refs, pathlib.Path('chrome/browser/foo.cc'), MOCK_ISOLATE_MAP)
    self.assertEqual(suites, ['foo_tests'])

  def test_extract_test_suites_no_match(self):
    refs = ['//unrelated:target']
    suites = test_suite_mapper.extract_test_suites(
        refs, pathlib.Path('chrome/browser/foo.cc'), MOCK_ISOLATE_MAP)
    self.assertEqual(suites, [])

  def test_extract_test_suites_all_zero_score(self):
    refs = ['//media/midi:midi_unittests', '//foo/bar:baz_tests']
    suites = test_suite_mapper.extract_test_suites(
        refs, pathlib.Path('chrome/browser/foo.cc'), MOCK_ISOLATE_MAP)
    self.assertEqual(sorted(suites), ['baz_tests', 'midi_unittests'])

  def test_verify_build_dir_valid(self):
    self.fs.create_file(
        '/src/out/Default/args.gn',
        contents=('# This is a comment\n'
                  'target_os = "linux"\n'
                  '\n'
                  'use_goma = true\n'),
    )
    success, err = test_suite_mapper.verify_build_dir('/src', 'out/Default')
    self.assertTrue(success)
    self.assertIsNone(err)

  def test_verify_build_dir_missing_file(self):
    self.fs.create_dir('/src')
    success, err = test_suite_mapper.verify_build_dir('/src', 'out/Default')
    self.assertFalse(success)
    self.assertIn('args.gn not found', err)

  def test_verify_build_dir_invalid_os(self):
    self.fs.create_file('/src/out/Default/args.gn',
                        contents='target_os = "android"\n')
    success, err = test_suite_mapper.verify_build_dir('/src', 'out/Default')
    self.assertFalse(success)
    self.assertIn('Unsupported target_os: android', err)

  @patch('sys.platform', 'linux')
  def test_verify_build_dir_no_target_os_on_linux(self):
    self.fs.create_file('/src/out/Default/args.gn',
                        contents='is_debug = true\n')
    success, err = test_suite_mapper.verify_build_dir('/src', 'out/Default')
    self.assertTrue(success)
    self.assertIsNone(err)

  @patch('sys.platform', 'darwin')
  def test_verify_build_dir_no_target_os_on_mac(self):
    self.fs.create_file('/src/out/Default/args.gn',
                        contents='is_debug = true\n')
    success, err = test_suite_mapper.verify_build_dir('/src', 'out/Default')
    self.assertFalse(success)
    self.assertIn('Host platform darwin is not Linux', err)

  def test_run_gn_refs_calls_gn(self):
    self.mock_run.return_value = MagicMock(returncode=0,
                                           stdout='//foo:foo_unittests\n',
                                           stderr='')
    suites, err = test_suite_mapper.run_gn_refs('/src', 'out/Default',
                                                pathlib.Path('foo/bar.cc'))
    self.assertEqual(suites, ['//foo:foo_unittests'])
    self.assertIsNone(err)
    self.mock_run.assert_called_once_with(
        [
            'gn',
            'refs',
            'out/Default',
            '--all',
            '//foo/bar.cc',
        ],
        capture_output=True,
        text=True,
        cwd='/src',
        check=False,
        encoding='utf-8',
    )

  def test_run_gn_refs_failure(self):
    self.mock_run.return_value = MagicMock(returncode=1,
                                           stdout='',
                                           stderr='GN error message\n')
    suites, err = test_suite_mapper.run_gn_refs('/src', 'out/Default',
                                                pathlib.Path('foo/bar.cc'))
    self.assertIsNone(suites)
    self.assertEqual(err, 'GN error message')

  def test_run_gn_refs_exception(self):
    self.mock_run.side_effect = OSError('Command not found')
    suites, err = test_suite_mapper.run_gn_refs('/src', 'out/Default',
                                                pathlib.Path('foo/bar.cc'))
    self.assertIsNone(suites)
    self.assertEqual(err, 'Command not found')

  def test_get_build_gn_paths(self):
    refs = [
        '//chrome/browser:foo_tests',
        '//content/test:bar_tests',
        '//:blink_wpt_tests',
        'invalid_target',
    ]
    paths = test_suite_mapper.get_build_gn_paths(refs)
    self.assertEqual(
        paths,
        ['BUILD.gn', 'chrome/browser/BUILD.gn', 'content/test/BUILD.gn'],
    )

  def test_check_graph_stability_stable(self):
    self.mock_run.return_value = MagicMock(returncode=0, stdout='')
    refs = ['//chrome/browser:foo_tests']
    stable, changed = test_suite_mapper.check_graph_stability(
        '/src', 'abc123f', 'chrome/browser/foo.cc', refs)
    self.assertTrue(stable)
    self.assertEqual(changed, [])
    self.mock_run.assert_called_once_with(
        [
            'git',
            'diff',
            '--name-only',
            'abc123f..HEAD',
            '--',
            'chrome/browser/foo.cc',
            'chrome/browser/BUILD.gn',
        ],
        capture_output=True,
        text=True,
        cwd='/src',
        check=False,
        encoding='utf-8',
    )

  def test_check_graph_stability_unstable(self):
    self.mock_run.return_value = MagicMock(
        returncode=0,
        stdout='chrome/browser/BUILD.gn\n',
    )
    refs = ['//chrome/browser:foo_tests']
    stable, changed = test_suite_mapper.check_graph_stability(
        '/src', 'abc123f', 'chrome/browser/foo.cc', refs)
    self.assertFalse(stable)
    self.assertEqual(changed, ['chrome/browser/BUILD.gn'])

  def test_check_graph_stability_git_fail(self):
    self.mock_run.return_value = MagicMock(returncode=1,
                                           stderr='Git diff error')
    f = io.StringIO()
    with contextlib.redirect_stderr(f):
      stable, changed = test_suite_mapper.check_graph_stability(
          '/src', 'abc123f', 'chrome/browser/foo.cc', ['//chrome:foo'])
    self.assertFalse(stable)
    self.assertEqual(changed, [])
    self.assertIn('Stability check failed', f.getvalue())

  def test_get_ignored_paths(self):
    self.mock_run.return_value = MagicMock(
        returncode=0,
        stdout=(
            '!! out/\n!! build/config/gclient_args.gni\n?? untracked_file\n'),
    )
    ignored = test_suite_mapper.get_ignored_paths('/src')
    self.assertEqual(ignored, ['out', 'build/config/gclient_args.gni'])

  def test_find_nested_repos_and_symlinks(self):
    self.fs.create_dir('/src/out')
    self.fs.create_dir('/src/.git')
    self.fs.create_dir('/src/third_party/foo')
    self.fs.create_file('/src/third_party/foo/.git')
    self.fs.create_dir('/src/third_party/bar/.git')
    self.fs.create_dir('/src/third_party/baz')

    # Deep nested repo (depth > 5)
    self.fs.create_file('/src/a/b/c/d/e/f/.git')

    # Symlinks
    self.fs.create_file('/src/third_party/baz/real_file')
    self.fs.create_symlink('/src/third_party/link_to_file',
                           '/src/third_party/baz/real_file')
    self.fs.create_symlink('/src/third_party/link_to_dir',
                           '/src/third_party/baz')
    self.fs.create_symlink('/src/out/bad_link',
                           '/src/third_party/baz/real_file')

    # Symlink inside a nested repo should be skipped
    self.fs.create_symlink('/src/third_party/foo/nested_link',
                           '/src/third_party/baz/real_file')

    targets = test_suite_mapper.find_nested_repos_and_symlinks('/src')
    expected = [
        'third_party/bar',
        'third_party/foo',
        'third_party/link_to_dir',
        'third_party/link_to_file',
    ]
    self.assertEqual(sorted(targets), expected)

  def test_stub_missing_target(self):
    wt_path = pathlib.Path('/wt')
    self.fs.create_dir('/wt')
    target_ref = '//third_party/foo:bar'
    success = test_suite_mapper._stub_missing_target(wt_path, target_ref)
    self.assertTrue(success)

    build_gn_path = wt_path / 'third_party' / 'foo' / 'BUILD.gn'
    self.assertTrue(build_gn_path.exists())
    content = build_gn_path.read_text(encoding='utf-8')
    self.assertIn('group("bar") {}', content)

  def test_stub_missing_target_failure(self):
    wt_path = pathlib.Path('/wt')
    self.fs.create_file('/wt/third_party')
    target_ref = '//third_party/foo:bar'
    f = io.StringIO()
    with contextlib.redirect_stderr(f):
      success = test_suite_mapper._stub_missing_target(wt_path, target_ref)
    self.assertFalse(success)
    self.assertTrue(len(f.getvalue()) > 0)

  def test_stub_missing_target_invalid_ref(self):
    wt_path = pathlib.Path('/wt')
    self.assertFalse(test_suite_mapper._stub_missing_target(wt_path, 'foo:bar'))
    self.assertFalse(
        test_suite_mapper._stub_missing_target(wt_path, '//foo/bar'))

  def test_spoof_rust_version_success(self):
    wt_path = pathlib.Path('/wt')
    self.fs.create_file(
        '/wt/tools/rust/update_rust.py',
        contents=("RUST_REVISION = 'aaaa05163abcbd08948b3efab796c543ba1ea687'\n"
                  "RUST_SUB_REVISION = 2\n"),
    )
    src_dir = pathlib.Path('/src_rust')
    self.fs.create_file(
        '/src_rust/VERSION',
        contents=('rustc 1.96.0 4c4205163abcbd08948b3efab796c543ba1ea687 '
                  '(4c4205163abcbd08948b3efab796c543ba1ea687-5-'
                  'llvmorg-23-init-10931-g20b6ec66 chromium)\n'),
    )
    dest_dir = pathlib.Path('/dest_rust')
    self.fs.create_dir('/dest_rust')

    success = test_suite_mapper._spoof_rust_version(wt_path, src_dir, dest_dir)
    self.assertTrue(success)

    dest_version = dest_dir / 'VERSION'
    self.assertTrue(dest_version.exists())
    content = dest_version.read_text(encoding='utf-8')
    self.assertEqual(
        content,
        'rustc 1.96.0 aaaa05163abcbd08948b3efab796c543ba1ea687 '
        '(aaaa05163abcbd08948b3efab796c543ba1ea687-2-'
        'llvmorg-23-init-10931-g20b6ec66 chromium)\n',
    )

  def test_spoof_rust_version_missing_update_rust(self):
    wt_path = pathlib.Path('/wt')
    self.fs.create_dir('/wt')
    self.assertFalse(
        test_suite_mapper._spoof_rust_version(wt_path, pathlib.Path('/src'),
                                              pathlib.Path('/dest')))

  def test_spoof_rust_version_invalid_update_rust(self):
    wt_path = pathlib.Path('/wt')
    self.fs.create_file('/wt/tools/rust/update_rust.py',
                        contents='invalid content\n')
    self.assertFalse(
        test_suite_mapper._spoof_rust_version(wt_path, pathlib.Path('/src'),
                                              pathlib.Path('/dest')))

  def test_spoof_rust_version_missing_src_version(self):
    wt_path = pathlib.Path('/wt')
    self.fs.create_file(
        '/wt/tools/rust/update_rust.py',
        contents="RUST_REVISION = 'aaaa'\nRUST_SUB_REVISION = 1\n",
    )
    self.assertFalse(
        test_suite_mapper._spoof_rust_version(wt_path, pathlib.Path('/src'),
                                              pathlib.Path('/dest')))

  def test_spoof_rust_version_invalid_src_version_format(self):
    wt_path = pathlib.Path('/wt')
    self.fs.create_file(
        '/wt/tools/rust/update_rust.py',
        contents="RUST_REVISION = 'aaaa'\nRUST_SUB_REVISION = 1\n",
    )
    src_dir = pathlib.Path('/src')
    self.fs.create_file(src_dir / 'VERSION', contents='invalid format\n')
    self.assertFalse(
        test_suite_mapper._spoof_rust_version(wt_path, src_dir,
                                              pathlib.Path('/dest')))

  @patch('test_suite_mapper.get_ignored_paths')
  @patch('test_suite_mapper.find_nested_repos_and_symlinks')
  def test_setup_worktree_symlinks(self, mock_find, mock_get_ignored):
    mock_get_ignored.return_value = ['third_party/llvm-build', 'out']
    mock_find.return_value = ['third_party/foo', 'third_party/link_to_file']

    self.fs.create_dir('/src')
    self.fs.create_dir('/src/third_party/llvm-build')
    self.fs.create_dir('/src/third_party/foo')
    self.fs.create_file('/src/third_party/link_to_file')
    self.fs.create_dir('/src/out/Default')
    self.fs.create_file('/src/out/Default/args.gn',
                        contents='target_os = "linux"\n')
    self.fs.create_file('/src/out/Default/other_file', contents='ignored\n')

    self.fs.create_dir('/wt')

    test_suite_mapper._setup_worktree_symlinks(pathlib.Path('/src'),
                                               pathlib.Path('/wt'),
                                               'out/Default')

    self.assertTrue(pathlib.Path('/wt/third_party/llvm-build').is_symlink())
    self.assertTrue(pathlib.Path('/wt/third_party/foo').is_symlink())
    self.assertTrue(pathlib.Path('/wt/third_party/link_to_file').is_symlink())
    self.assertFalse(pathlib.Path('/wt/out').is_symlink())
    self.assertFalse(pathlib.Path('/wt/out').exists())

  @patch('test_suite_mapper.get_ignored_paths')
  @patch('test_suite_mapper.find_nested_repos_and_symlinks')
  @patch('test_suite_mapper._spoof_rust_version')
  def test_setup_worktree_symlinks_rust_toolchain(self, mock_spoof, mock_find,
                                                  mock_get_ignored):
    mock_get_ignored.return_value = [
        test_suite_mapper.RUST_TOOLCHAIN_PATH.as_posix()
    ]
    mock_find.return_value = []
    mock_spoof.return_value = True

    self.fs.create_dir('/src')
    rust_dir = pathlib.Path('/src') / test_suite_mapper.RUST_TOOLCHAIN_PATH
    self.fs.create_file(rust_dir / 'bin/rustc')
    self.fs.create_file(rust_dir / 'VERSION')

    self.fs.create_dir('/wt')
    self.fs.create_file(
        pathlib.Path('/wt') / test_suite_mapper.UPDATE_RUST_PATH)

    test_suite_mapper._setup_worktree_symlinks(pathlib.Path('/src'),
                                               pathlib.Path('/wt'),
                                               'out/Default')

    link_rust_dir = pathlib.Path('/wt') / test_suite_mapper.RUST_TOOLCHAIN_PATH
    self.assertTrue((link_rust_dir / 'bin').is_symlink())
    self.assertTrue((link_rust_dir / 'bin/rustc').exists())
    self.assertFalse((link_rust_dir / 'VERSION').exists())
    mock_spoof.assert_called_once()

  def test_run_gn_gen_with_stubbing_success_immediate(self):
    self.mock_run.return_value = MagicMock(returncode=0)
    success = test_suite_mapper._run_gn_gen_with_stubbing(
        pathlib.Path('/wt'), 'out/Default')
    self.assertTrue(success)
    self.mock_run.assert_called_once_with(
        ['gn', 'gen', 'out/Default'],
        capture_output=True,
        text=True,
        cwd=pathlib.Path('/wt'),
        check=False,
        encoding='utf-8',
    )

  def test_run_gn_gen_with_stubbing_retry_success(self):
    self.mock_run.side_effect = [
        MagicMock(
            returncode=1,
            stdout=('ERROR Unresolved dependencies.\n  //foo:bar needs'
                    ' //third_party/baz:qux\n'),
            stderr='',
        ),
        MagicMock(returncode=0),
    ]
    self.fs.create_dir('/wt')
    success = test_suite_mapper._run_gn_gen_with_stubbing(
        pathlib.Path('/wt'), 'out/Default')
    self.assertTrue(success)
    self.assertEqual(self.mock_run.call_count, 2)

    stub_path = pathlib.Path('/wt/third_party/baz/BUILD.gn')
    self.assertTrue(stub_path.exists())
    self.assertIn('group("qux") {}', stub_path.read_text(encoding='utf-8'))

  def test_run_gn_gen_with_stubbing_failure_loop(self):
    self.mock_run.return_value = MagicMock(
        returncode=1,
        stdout=('ERROR Unresolved dependencies.\n  //foo:bar needs'
                ' //third_party/baz:qux\n'),
        stderr='',
    )
    self.fs.create_dir('/wt')
    f = io.StringIO()
    with contextlib.redirect_stderr(f):
      success = test_suite_mapper._run_gn_gen_with_stubbing(
          pathlib.Path('/wt'), 'out/Default')
    self.assertFalse(success)
    self.assertIn('already stubbed', f.getvalue())

  def test_run_heavyweight_resolution_success(self):
    isolate_map_content = (
        "{'browser_tests': {'label': '//chrome/test:browser_tests'}}")

    def mock_run_side_effect(cmd, *args, **kwargs):
      if 'worktree' in cmd and 'add' in cmd:
        wt_path = cmd[5]
        self.fs.create_file(
            pathlib.Path(wt_path) / test_suite_mapper.ISOLATE_MAP_PATH,
            contents=isolate_map_content,
        )
        return MagicMock(returncode=0)
      elif 'gen' in cmd:
        return MagicMock(returncode=0)
      elif 'refs' in cmd:
        return MagicMock(
            returncode=0,
            stdout='//chrome/test:browser_tests\n',
            stderr='',
        )
      return MagicMock(returncode=0)

    self.mock_run.side_effect = mock_run_side_effect

    self.fs.create_dir('/src')
    self.fs.create_file('/src/out/Default/args.gn',
                        contents='target_os = "linux"\n')

    with patch('test_suite_mapper.get_ignored_paths', return_value=[]), patch(
        'test_suite_mapper.find_nested_repos_and_symlinks', return_value=[]):
      suites = test_suite_mapper.run_heavyweight_resolution(
          pathlib.Path('/src'),
          'abc123f',
          'out/Default',
          pathlib.Path('chrome/browser/foo.cc'),
      )

    self.assertEqual(suites, ['browser_tests'])

    # Verify worktree add and remove were called with the same path
    add_path = None
    remove_path = None
    for call in self.mock_run.call_args_list:
      cmd = call[0][0]
      if 'worktree' in cmd and 'add' in cmd:
        add_path = cmd[5]
      if 'worktree' in cmd and 'remove' in cmd:
        remove_path = cmd[4]

    self.assertIsNotNone(add_path)
    self.assertIsNotNone(remove_path)
    self.assertEqual(add_path, remove_path)

  def test_run_heavyweight_resolution_worktree_fail(self):
    self.mock_run.return_value = MagicMock(returncode=1, stderr='Git error')

    f = io.StringIO()
    with contextlib.redirect_stderr(f):
      suites = test_suite_mapper.run_heavyweight_resolution(
          pathlib.Path('/src'),
          'abc123f',
          'out/Default',
          pathlib.Path('chrome/browser/foo.cc'),
      )
    self.assertIsNone(suites)
    self.assertIn('Error creating git worktree', f.getvalue())

  def test_run_heavyweight_resolution_gn_gen_fail(self):
    self.mock_run.side_effect = [
        MagicMock(returncode=0),  # worktree add
        MagicMock(returncode=1, stdout='GN gen error',
                  stderr=''),  # gn gen fail
        MagicMock(returncode=0),  # worktree remove
    ]
    self.fs.create_dir('/src')
    self.fs.create_file('/src/out/Default/args.gn',
                        contents='target_os = "linux"\n')
    f = io.StringIO()
    with patch('test_suite_mapper.get_ignored_paths',
               return_value=[]), patch(
                   'test_suite_mapper.find_nested_repos_and_symlinks',
                   return_value=[]), contextlib.redirect_stderr(f):
      suites = test_suite_mapper.run_heavyweight_resolution(
          pathlib.Path('/src'),
          'abc123f',
          'out/Default',
          pathlib.Path('chrome/browser/foo.cc'),
      )
    self.assertIsNone(suites)

  def test_run_heavyweight_resolution_gn_refs_fail(self):
    self.mock_run.side_effect = [
        MagicMock(returncode=0),  # worktree add
        MagicMock(returncode=0),  # gn gen
        MagicMock(returncode=1, stdout='',
                  stderr='GN refs error'),  # gn refs fail
        MagicMock(returncode=0),  # worktree remove
    ]
    self.fs.create_dir('/src')
    self.fs.create_file('/src/out/Default/args.gn',
                        contents='target_os = "linux"\n')
    f = io.StringIO()
    with patch('test_suite_mapper.get_ignored_paths',
               return_value=[]), patch(
                   'test_suite_mapper.find_nested_repos_and_symlinks',
                   return_value=[]), contextlib.redirect_stderr(f):
      suites = test_suite_mapper.run_heavyweight_resolution(
          pathlib.Path('/src'),
          'abc123f',
          'out/Default',
          pathlib.Path('chrome/browser/foo.cc'),
      )
    self.assertIsNone(suites)

  def test_run_heavyweight_resolution_missing_isolate_map(self):
    self.mock_run.side_effect = [
        MagicMock(returncode=0),  # worktree add
        MagicMock(returncode=0),  # gn gen
        MagicMock(returncode=0, stdout='//foo:bar\n', stderr=''),  # gn refs
        MagicMock(returncode=0),  # worktree remove
    ]
    self.fs.create_dir('/src')
    self.fs.create_file('/src/out/Default/args.gn',
                        contents='target_os = "linux"\n')
    f = io.StringIO()
    with patch('test_suite_mapper.get_ignored_paths',
               return_value=[]), patch(
                   'test_suite_mapper.find_nested_repos_and_symlinks',
                   return_value=[]), contextlib.redirect_stderr(f):
      suites = test_suite_mapper.run_heavyweight_resolution(
          pathlib.Path('/src'),
          'abc123f',
          'out/Default',
          pathlib.Path('chrome/browser/foo.cc'),
      )
    self.assertIsNone(suites)
    self.assertIn('Isolate map not found in worktree', f.getvalue())

  def test_load_isolate_map_for_args_with_file(self):
    content = "{'suite1': {'label': '//foo:bar'}}"
    self.fs.create_file('/src/map.pyl', contents=content)
    res = test_suite_mapper._load_isolate_map_for_args('/src', '/src/map.pyl',
                                                       None, False)
    self.assertEqual(res, {'//foo:bar': 'suite1'})

    with self.assertRaises(SystemExit):
      test_suite_mapper._load_isolate_map_for_args('/src', '/src/missing.pyl',
                                                   None, False)

  def test_load_isolate_map_for_args_revision(self):
    with self.assertRaises(SystemExit):
      test_suite_mapper._load_isolate_map_for_args('/src', None, 'abc123f',
                                                   False)

    res = test_suite_mapper._load_isolate_map_for_args('/src', None, 'abc123f',
                                                       True)
    self.assertIsNone(res)

  def test_load_isolate_map_for_args_default(self):
    with patch('test_suite_mapper.load_isolate_map',
               return_value={'a': 'b'}) as mock_load:
      res = test_suite_mapper._load_isolate_map_for_args(
          '/src', None, None, False)
      self.assertEqual(res, {'a': 'b'})
      mock_load.assert_called_once_with('/src')

  def test_check_and_warn_stability_stable(self):
    with patch('test_suite_mapper.check_graph_stability',
               return_value=(True, [])):
      stable = test_suite_mapper._check_and_warn_stability(
          '/src', 'abc123f', pathlib.Path('foo.cc'), ['//foo'])
      self.assertTrue(stable)

  def test_check_and_warn_stability_unstable(self):
    with patch(
        'test_suite_mapper.check_graph_stability',
        return_value=(False, ['BUILD.gn']),
    ):
      f = io.StringIO()
      with contextlib.redirect_stderr(f):
        stable = test_suite_mapper._check_and_warn_stability(
            '/src', 'abc123f', pathlib.Path('foo.cc'), ['//foo'])
      self.assertFalse(stable)
      self.assertIn('Warning: The GN graph or the file may have changed',
                    f.getvalue())
      self.assertIn('BUILD.gn', f.getvalue())

    with patch(
        'test_suite_mapper.check_graph_stability',
        return_value=(False, ['f1', 'f2', 'f3', 'f4', 'f5', 'f6']),
    ):
      f = io.StringIO()
      with contextlib.redirect_stderr(f):
        stable = test_suite_mapper._check_and_warn_stability(
            '/src', 'abc123f', pathlib.Path('foo.cc'), ['//foo'])
      self.assertFalse(stable)
      self.assertIn('f1, f2, f3, f4, f5 and 1 more', f.getvalue())

  def test_should_run_heavyweight(self):
    self.assertTrue(
        test_suite_mapper._should_run_heavyweight('/src', 'abc123f',
                                                  pathlib.Path('foo.cc'),
                                                  ['//foo'], True, True, []))

    self.assertFalse(
        test_suite_mapper._should_run_heavyweight('/src', 'abc123f',
                                                  pathlib.Path('foo.cc'),
                                                  ['//foo'], False, True, []))

    with patch('test_suite_mapper.load_isolate_map',
               return_value={'//foo': 'suite'}):
      self.assertFalse(
          test_suite_mapper._should_run_heavyweight('/src', 'abc123f',
                                                    pathlib.Path('foo.cc'),
                                                    ['//foo'], False, False,
                                                    ['suite']))

    with patch('test_suite_mapper.load_isolate_map',
               return_value={'//foo': 'suite2'}):
      f = io.StringIO()
      with contextlib.redirect_stderr(f):
        res = test_suite_mapper._should_run_heavyweight('/src', 'abc123f',
                                                        pathlib.Path('foo.cc'),
                                                        ['//foo'], False, False,
                                                        ['suite1'])
      self.assertTrue(res)
      self.assertIn('Difference detected', f.getvalue())

  @patch('test_suite_mapper.run_heavyweight_resolution')
  def test_execute_heavyweight_resolution_success(self, mock_resolve):
    mock_resolve.return_value = ['suite1']
    f = io.StringIO()
    with contextlib.redirect_stderr(f):
      res = test_suite_mapper._execute_heavyweight_resolution(
          '/src', 'abc123f', 'out/Default', pathlib.Path('foo.cc'), ['suite1'])
    self.assertEqual(res, ['suite1'])

    f = io.StringIO()
    with contextlib.redirect_stderr(f):
      res = test_suite_mapper._execute_heavyweight_resolution(
          '/src', 'abc123f', 'out/Default', pathlib.Path('foo.cc'), ['suite2'])
    self.assertEqual(res, ['suite1'])
    self.assertIn('Results corrected', f.getvalue())

  @patch('test_suite_mapper.run_heavyweight_resolution')
  def test_execute_heavyweight_resolution_fail(self, mock_resolve):
    mock_resolve.return_value = None
    f = io.StringIO()
    with contextlib.redirect_stderr(f):
      res = test_suite_mapper._execute_heavyweight_resolution(
          '/src', 'abc123f', 'out/Default', pathlib.Path('foo.cc'), ['suite1'])
    self.assertEqual(res, ['suite1'])
    self.assertIn('Falling back to lightweight results', f.getvalue())

  def test_execute_heavyweight_resolution_no_rev(self):
    with self.assertRaises(SystemExit):
      test_suite_mapper._execute_heavyweight_resolution('/src', None,
                                                        'out/Default',
                                                        pathlib.Path('foo.cc'),
                                                        [])


if __name__ == '__main__':
  unittest.main()
