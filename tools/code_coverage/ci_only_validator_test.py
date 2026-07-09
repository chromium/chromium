#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import io
import json
import os
import pathlib
import subprocess
import sys
import textwrap
import unittest
from unittest import mock
from pyfakefs import fake_filesystem_unittest

import ci_only_validator


class TestCIOnlyValidator(fake_filesystem_unittest.TestCase):

  def setUp(self):
    # Resolve the path on the real filesystem first before pyfakefs is active.
    self.real_src_root = (pathlib.Path(
        ci_only_validator.__file__).resolve().parents[2])
    self.setUpPyfakefs()
    self.fs.create_dir(self.real_src_root)

    # Set up global mocking for subprocess.run.
    self.subprocess_run_patcher = mock.patch('subprocess.run')
    self.mock_run = self.subprocess_run_patcher.start()
    self.addCleanup(self.subprocess_run_patcher.stop)

  def test_discover_trybots_for_test_suite(self):
    self.mock_run.return_value = mock.MagicMock(
        returncode=0,
        stdout=textwrap.dedent("""\

            infra/config/generated/builders/try/android-code-coverage/targets/chromium.coverage.json
            infra/config/generated/builders/try/android-x86-code-coverage/targets/chromium.coverage.json
            """),
    )

    src_root = pathlib.Path('/mock/src')
    entries = ci_only_validator.discover_trybots_for_test_suite(
        src_root, 'chrome_junit_tests')
    self.assertEqual(
        entries,
        [
            ci_only_validator.TrybotEntry('android-code-coverage',
                                          'chromium.coverage'),
            ci_only_validator.TrybotEntry('android-x86-code-coverage',
                                          'chromium.coverage'),
        ],
    )

  def test_discover_trybots_for_test_suite_error(self):
    self.mock_run.return_value = mock.MagicMock(returncode=1)
    src_root = pathlib.Path('/mock/src')
    entries = ci_only_validator.discover_trybots_for_test_suite(
        src_root, 'chrome_junit_tests')
    self.assertEqual(entries, [])

  def test_discover_trybots_for_test_suite_with_revision(self):
    self.mock_run.return_value = mock.MagicMock(
        returncode=0,
        stdout=textwrap.dedent("""\
            myrev:infra/config/generated/builders/try/android-code-coverage/targets/chromium.coverage.json
            """),
    )

    src_root = pathlib.Path('/mock/src')
    entries = ci_only_validator.discover_trybots_for_test_suite(
        src_root, 'chrome_junit_tests', 'myrev')
    self.assertEqual(
        entries,
        [
            ci_only_validator.TrybotEntry('android-code-coverage',
                                          'chromium.coverage')
        ],
    )

  def test_read_local_success(self):
    src_root = pathlib.Path('/mock/src')
    dummy_file = src_root / 'dummy_path'
    self.fs.create_file(dummy_file, contents='file_content')

    content = ci_only_validator.read_local(src_root, 'dummy_path')
    self.assertEqual(content, 'file_content')

  def test_read_local_not_found(self):
    src_root = pathlib.Path('/mock/src')
    content = ci_only_validator.read_local(src_root, 'non_existent_file')
    self.assertIsNone(content)

  def test_read_local_os_error(self):
    src_root = pathlib.Path('/mock/src')
    dummy_file = src_root / 'dummy_path'
    # Create an unreadable file to raise OSError/PermissionError natively.
    self.fs.create_file(dummy_file, contents='')
    os.chmod(dummy_file, 0o000)

    # Redirect stderr to suppress prints in test output
    with mock.patch('sys.stderr', new=io.StringIO()):
      content = ci_only_validator.read_local(src_root, 'dummy_path')
      self.assertIsNone(content)

  def test_read_git_success(self):
    self.mock_run.return_value = mock.MagicMock(returncode=0,
                                                stdout='git_file_content')
    src_root = pathlib.Path('/mock/src')

    content = ci_only_validator.read_git(src_root, 'myrev', 'dummy_path')
    self.assertEqual(content, 'git_file_content')

  def test_read_git_error(self):
    self.mock_run.return_value = mock.MagicMock(returncode=1)
    src_root = pathlib.Path('/mock/src')

    content = ci_only_validator.read_git(src_root, 'myrev', 'dummy_path')
    self.assertIsNone(content)

  def test_check_coverage_enabled_true(self):
    mock_read = mock.MagicMock(
        return_value=json.dumps({'gn_args': {
            'use_clang_coverage': True
        }}))
    enabled = ci_only_validator.check_coverage_enabled(mock_read,
                                                       'android-code-coverage')
    self.assertTrue(enabled)

  def test_check_coverage_enabled_false(self):
    mock_read = mock.MagicMock(
        return_value=json.dumps({'gn_args': {
            'use_clang_coverage': False
        }}))
    enabled = ci_only_validator.check_coverage_enabled(mock_read,
                                                       'android-code-coverage')
    self.assertFalse(enabled)

  def test_check_coverage_enabled_none_content(self):
    mock_read = mock.MagicMock(return_value=None)
    enabled = ci_only_validator.check_coverage_enabled(mock_read,
                                                       'android-code-coverage')
    self.assertFalse(enabled)

  def test_check_coverage_enabled_invalid_json(self):
    mock_read = mock.MagicMock(return_value='{invalid_json')
    enabled = ci_only_validator.check_coverage_enabled(mock_read,
                                                       'android-code-coverage')
    self.assertFalse(enabled)

  def test_check_trybot_coverage_config_ci_only_true(self):
    targets_json = {
        'android-code-coverage': {
            'isolated_scripts': [{
                'name': 'chrome_junit_tests',
                'ci_only': True
            }]
        }
    }
    mock_read = mock.MagicMock(return_value=json.dumps(targets_json))

    results = ci_only_validator.check_trybot_coverage_config(
        mock_read,
        'android-code-coverage',
        'chromium.coverage',
        'chrome_junit_tests',
    )
    self.assertEqual(
        results,
        [ci_only_validator.BuilderConfig('android-code-coverage', True)],
    )

  def test_check_trybot_coverage_config_ci_only_false(self):
    targets_json = {
        'android-code-coverage': {
            'isolated_scripts': [{
                'name': 'chrome_junit_tests',
                'ci_only': False
            }]
        }
    }
    mock_read = mock.MagicMock(return_value=json.dumps(targets_json))

    results = ci_only_validator.check_trybot_coverage_config(
        mock_read,
        'android-code-coverage',
        'chromium.coverage',
        'chrome_junit_tests',
    )
    self.assertEqual(
        results,
        [ci_only_validator.BuilderConfig('android-code-coverage', False)],
    )

  def test_check_trybot_coverage_config_ci_only_omitted(self):
    # Tests that when 'ci_only' is omitted entirely, it defaults to False.
    targets_json = {
        'android-code-coverage': {
            'isolated_scripts': [{
                'name': 'chrome_junit_tests'
            }]
        }
    }
    mock_read = mock.MagicMock(return_value=json.dumps(targets_json))

    results = ci_only_validator.check_trybot_coverage_config(
        mock_read,
        'android-code-coverage',
        'chromium.coverage',
        'chrome_junit_tests',
    )
    self.assertEqual(
        results,
        [ci_only_validator.BuilderConfig('android-code-coverage', False)],
    )

  def test_check_trybot_coverage_config_none_content(self):
    mock_read = mock.MagicMock(return_value=None)
    results = ci_only_validator.check_trybot_coverage_config(
        mock_read,
        'android-code-coverage',
        'chromium.coverage',
        'chrome_junit_tests',
    )
    self.assertEqual(results, [])

  def test_check_trybot_coverage_config_invalid_json(self):
    mock_read = mock.MagicMock(return_value='{invalid_json')
    # Redirect stderr to suppress prints in test output
    with mock.patch('sys.stderr', new=io.StringIO()):
      results = ci_only_validator.check_trybot_coverage_config(
          mock_read,
          'android-code-coverage',
          'chromium.coverage',
          'chrome_junit_tests',
      )
      self.assertEqual(results, [])

  @mock.patch('ci_only_validator.discover_trybots_for_test_suite')
  def test_main_success_disk(self, mock_discover):
    mock_discover.return_value = [
        ci_only_validator.TrybotEntry('android-code-coverage',
                                      'chromium.coverage')
    ]

    targets_json = {
        'android-code-coverage': {
            'isolated_scripts': [{
                'name': 'chrome_junit_tests',
                'ci_only': True
            }]
        }
    }
    # Create the files on the fake filesystem relative to real_src_root
    targets_file = (
        self.real_src_root /
        'infra/config/generated/builders/try/android-code-coverage/targets/'
        'chromium.coverage.json')
    self.fs.create_file(targets_file, contents=json.dumps(targets_json))

    gn_args_file = (self.real_src_root /
                    'infra/config/generated/builders/try/android-code-coverage/'
                    'gn-args.json')
    self.fs.create_file(
        gn_args_file,
        contents=json.dumps({'gn_args': {
            'use_clang_coverage': True
        }}),
    )

    test_args = ['ci_only_validator.py', 'chrome_junit_tests']
    with mock.patch('sys.argv', test_args), \
         mock.patch('sys.stdout', new=io.StringIO()) as mock_stdout:
      ci_only_validator.main()
      output = mock_stdout.getvalue()
      self.assertIn("Checking test suite 'chrome_junit_tests'", output)
      self.assertIn('android-code-coverage', output)
      self.assertIn('ci_only = True', output)
      self.assertIn('Coverage: Enabled', output)

  @mock.patch('ci_only_validator.discover_trybots_for_test_suite')
  def test_main_success_git(self, mock_discover):
    mock_discover.return_value = [
        ci_only_validator.TrybotEntry('android-code-coverage',
                                      'chromium.coverage')
    ]

    targets_json = {
        'android-code-coverage': {
            'isolated_scripts': [{
                'name': 'chrome_junit_tests',
                'ci_only': False
            }]
        }
    }

    # Setup mock subprocess runs for git show
    def mock_run_side_effect(args, **kwargs):
      # git rev-parse --verify myrev
      if 'rev-parse' in args:
        return mock.MagicMock(returncode=0)

      # git show myrev:path/to/targets/chromium.coverage.json
      if 'show' in args:
        rel_path = args[2].split(':', 1)[1]
        if 'targets' in rel_path:
          return mock.MagicMock(returncode=0, stdout=json.dumps(targets_json))
        if 'gn-args.json' in rel_path:
          return mock.MagicMock(
              returncode=0,
              stdout=json.dumps({'gn_args': {
                  'use_jacoco_coverage': True
              }}),
          )
      return mock.MagicMock(returncode=1)

    self.mock_run.side_effect = mock_run_side_effect

    test_args = [
        'ci_only_validator.py',
        'chrome_junit_tests',
        '--revision',
        'myrev',
    ]
    with mock.patch('sys.argv', test_args), \
         mock.patch('sys.stdout', new=io.StringIO()) as mock_stdout:
      ci_only_validator.main()
      output = mock_stdout.getvalue()
      self.assertIn(
          "Checking test suite 'chrome_junit_tests' across 1 configured"
          ' trybots at revision myrev...',
          output,
      )
      self.assertIn('android-code-coverage', output)
      self.assertIn('ci_only = False', output)
      self.assertIn('Coverage: Enabled', output)

  def test_main_src_root_not_exists(self):
    test_args = ['ci_only_validator.py', 'chrome_junit_tests']
    with mock.patch('pathlib.Path.exists', return_value=False), \
         mock.patch('sys.argv', test_args), \
         mock.patch('sys.stderr', new=io.StringIO()) as mock_stderr:
      with self.assertRaises(SystemExit) as cm:
        ci_only_validator.main()
      self.assertEqual(cm.exception.code, 1)
      self.assertIn('Error: Src path', mock_stderr.getvalue())

  def test_main_invalid_revision(self):
    self.mock_run.return_value = mock.MagicMock(returncode=1)
    test_args = [
        'ci_only_validator.py',
        'chrome_junit_tests',
        '--revision',
        'invalidrev',
    ]
    with mock.patch('sys.argv', test_args), \
         mock.patch('sys.stderr', new=io.StringIO()) as mock_stderr:
      with self.assertRaises(SystemExit) as cm:
        ci_only_validator.main()
      self.assertEqual(cm.exception.code, 1)
      self.assertIn('Error: Invalid revision', mock_stderr.getvalue())

  @mock.patch('ci_only_validator.discover_trybots_for_test_suite')
  def test_main_no_trybots(self, mock_discover):
    mock_discover.return_value = []
    test_args = ['ci_only_validator.py', 'chrome_junit_tests']
    with mock.patch('sys.argv', test_args), \
         mock.patch('sys.stderr', new=io.StringIO()) as mock_stderr:
      with self.assertRaises(SystemExit) as cm:
        ci_only_validator.main()
      self.assertEqual(cm.exception.code, 0)
      self.assertIn("Warning: Test suite", mock_stderr.getvalue())


class TestCIOnlyValidatorExecutable(unittest.TestCase):

  def test_main_executable_run(self):
    import runpy

    script_path = pathlib.Path(ci_only_validator.__file__).resolve()
    test_args = ['ci_only_validator.py', '--help']
    with mock.patch('sys.argv', test_args), \
         mock.patch('sys.stdout', new=io.StringIO()) as mock_stdout:
      try:
        runpy.run_path(str(script_path), run_name='__main__')
      except SystemExit as e:
        self.assertEqual(e.code, 0)
      self.assertIn('Check if a test suite is barred by ci_only',
                    mock_stdout.getvalue())


if __name__ == '__main__':
  unittest.main()
