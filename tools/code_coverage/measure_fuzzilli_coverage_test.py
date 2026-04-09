#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for measure_fuzzilli_coverage.py."""

import os
import subprocess
import unittest
from unittest import mock
import measure_fuzzilli_coverage
from pathlib import Path

COVERAGE_SCRIPT_PATH = (f"{Path(__file__).resolve().parent.parents[1]}"
                        "/tools/code_coverage/coverage.py")

TEST_FUZZILLI_DIR = '/fake/fuzzilli'
TEST_BUILD_OUT_DIR = '/fake/out/Fuzzilli'
TEST_REPORT_OUT_DIR = '/fake/out/report'
TEST_MINUTES = 2
TEST_SECONDS = 120
TEST_PROFILE = "fakeProfile"
TEST_PROFRAW_DIR = '/tmp/fake_profraw_dir'


class MeasureFuzzilliCoverageTest(unittest.TestCase):

  @mock.patch.dict(os.environ, {}, clear=True)
  @mock.patch('subprocess.Popen')
  @mock.patch('subprocess.run')
  def test_RunFuzzilli(self, mock_run, mock_popen):
    mock_process = mock.Mock()
    mock_process.wait.side_effect = subprocess.TimeoutExpired(
        cmd='FuzzilliCli', timeout=TEST_SECONDS)
    mock_popen.return_value = mock_process
    mock_pg_handler = mock.Mock()

    success = measure_fuzzilli_coverage._RunFuzzilli(mock_pg_handler,
                                                     TEST_FUZZILLI_DIR,
                                                     TEST_BUILD_OUT_DIR,
                                                     TEST_MINUTES, TEST_PROFILE,
                                                     TEST_PROFRAW_DIR)

    self.assertTrue(success)
    self.assertEqual(os.environ.get("LLVM_PROFILE_FILE"),
                     os.path.join(TEST_PROFRAW_DIR, "fuzzilli.%4m%c.profraw"))

    mock_run.assert_called_with(['swift', 'build', '-c', 'release'],
                                check=True,
                                cwd=TEST_FUZZILLI_DIR)

    expected_run_command_prefix = [
        os.path.join(TEST_FUZZILLI_DIR, '.build/release/FuzzilliCli'),
        '--storagePath=/tmp/fuzzilli_storage', '--overwrite', '--engine=hybrid',
        '--profile=fakeProfile'
    ]

    self.assertTrue(mock_popen.called)
    actual_run_command = mock_popen.call_args[0][0]
    self.assertEqual(actual_run_command[:-1], expected_run_command_prefix)
    self.assertTrue(actual_run_command[-1].endswith('.sh'))

    self.assertEqual(mock_popen.call_args[1], {
        'cwd': TEST_FUZZILLI_DIR,
        'start_new_session': True
    })

    mock_process.wait.assert_called_with(timeout=TEST_SECONDS)
    mock_pg_handler.SetProcessGroup.assert_called_with(mock_process)
    mock_pg_handler.KillProcessGroup.assert_called_once()

  @mock.patch.dict(os.environ, {}, clear=True)
  @mock.patch('subprocess.Popen')
  @mock.patch('subprocess.run')
  def test_RunFuzzilliBuildFails(self, mock_run, mock_popen):
    mock_run.side_effect = subprocess.CalledProcessError(returncode=1,
                                                         cmd='swift build')
    mock_pg_handler = mock.Mock()

    success = measure_fuzzilli_coverage._RunFuzzilli(mock_pg_handler,
                                                     TEST_FUZZILLI_DIR,
                                                     TEST_BUILD_OUT_DIR,
                                                     TEST_MINUTES, TEST_PROFILE,
                                                     TEST_PROFRAW_DIR)

    self.assertFalse(success)
    mock_run.assert_called_with(['swift', 'build', '-c', 'release'],
                                check=True,
                                cwd=TEST_FUZZILLI_DIR)
    mock_popen.assert_not_called()
    mock_pg_handler.SetProcessGroup.assert_not_called()
    mock_pg_handler.KillProcessGroup.assert_not_called()

  @mock.patch.dict(os.environ, {}, clear=True)
  @mock.patch('subprocess.Popen')
  @mock.patch('subprocess.run')
  def test_RunFuzzilliEarlyExit(self, mock_run, mock_popen):
    mock_process = mock.Mock()
    # As if crash before timeout reached
    mock_process.returncode = 1
    mock_popen.return_value = mock_process
    mock_pg_handler = mock.Mock()

    success = measure_fuzzilli_coverage._RunFuzzilli(mock_pg_handler,
                                                     TEST_FUZZILLI_DIR,
                                                     TEST_BUILD_OUT_DIR,
                                                     TEST_MINUTES, TEST_PROFILE,
                                                     TEST_PROFRAW_DIR)

    self.assertFalse(success)
    mock_process.wait.assert_called_with(timeout=TEST_SECONDS)
    mock_pg_handler.SetProcessGroup.assert_called_with(mock_process)
    mock_pg_handler.KillProcessGroup.assert_not_called()

  def _assert_generate_coverage_report(self,
                                       mock_run,
                                       mock_glob,
                                       filters=None,
                                       ignore_regex=None):
    if not filters:
      filters = []

    mock_glob.return_value = [os.path.join(TEST_PROFRAW_DIR, '1.profraw')]

    measure_fuzzilli_coverage._GenerateCoverageReport(
        TEST_BUILD_OUT_DIR,
        TEST_REPORT_OUT_DIR,
        ignore_filename_regex=ignore_regex,
        filters=filters,
        profraw_dir=TEST_PROFRAW_DIR)

    expected_gen_profile_command = [
        'llvm-profdata', 'merge', '-o',
        os.path.join(TEST_PROFRAW_DIR, 'coverage.profdata'),
        os.path.join(TEST_PROFRAW_DIR, '1.profraw')
    ]

    expected_coverage_command = [
        COVERAGE_SCRIPT_PATH, 'js_in_process_fuzzer', '-b', TEST_BUILD_OUT_DIR,
        '-o', TEST_REPORT_OUT_DIR, '-p',
        os.path.join(TEST_PROFRAW_DIR,
                     'coverage.profdata'), '--no-component-view'
    ]

    for f in filters:
      expected_coverage_command.extend(['-f', f])
    if ignore_regex:
      expected_coverage_command.extend(['-i', ignore_regex])

    mock_run.assert_has_calls([
        mock.call(expected_gen_profile_command, check=True),
        mock.call(expected_coverage_command, check=True)
    ])

  @mock.patch('glob.glob')
  @mock.patch('subprocess.run')
  def test_GenerateCoverageReport(self, mock_run, mock_glob):
    self._assert_generate_coverage_report(mock_run, mock_glob)

  @mock.patch('glob.glob')
  @mock.patch('subprocess.run')
  def test_GenerateCoverageReportWithFilters(self, mock_run, mock_glob):
    self._assert_generate_coverage_report(mock_run,
                                          mock_glob,
                                          filters=['/fake/directory'])

  @mock.patch('glob.glob')
  @mock.patch('subprocess.run')
  def test_GenerateCoverageReportWithIgnoreRegex(self, mock_run, mock_glob):
    self._assert_generate_coverage_report(mock_run,
                                          mock_glob,
                                          ignore_regex='.*out.*')

  @mock.patch('glob.glob')
  @mock.patch('subprocess.run')
  def test_GenerateCoverageReportWithFiltersAndIgnoreRegex(
      self, mock_run, mock_glob):
    self._assert_generate_coverage_report(mock_run,
                                          mock_glob,
                                          filters=['/fake/directory'],
                                          ignore_regex='.*out.*')

  @mock.patch('glob.glob')
  @mock.patch('subprocess.run')
  def test_GenerateCoverageReportFails(self, mock_run, mock_glob):
    mock_glob.return_value = [os.path.join(TEST_PROFRAW_DIR, '1.profraw')]
    mock_run.side_effect = subprocess.CalledProcessError(returncode=1,
                                                         cmd='llvm-profdata')

    success = measure_fuzzilli_coverage._GenerateCoverageReport(
        TEST_BUILD_OUT_DIR,
        TEST_REPORT_OUT_DIR,
        ignore_filename_regex=None,
        filters=None,
        profraw_dir=TEST_PROFRAW_DIR)

    self.assertFalse(success)

  @mock.patch('glob.glob')
  @mock.patch('subprocess.run')
  def test_GenerateCoverageReportFailsFileNotFoundError(self, mock_run,
                                                        mock_glob):
    mock_glob.return_value = [os.path.join(TEST_PROFRAW_DIR, '1.profraw')]
    mock_run.side_effect = FileNotFoundError()

    success = measure_fuzzilli_coverage._GenerateCoverageReport(
        TEST_BUILD_OUT_DIR,
        TEST_REPORT_OUT_DIR,
        ignore_filename_regex=None,
        filters=None,
        profraw_dir=TEST_PROFRAW_DIR)

    self.assertFalse(success)


if __name__ == '__main__':
  unittest.main()
