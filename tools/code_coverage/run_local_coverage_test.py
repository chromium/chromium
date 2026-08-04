#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for run_local_coverage.py."""

import contextlib
import io
import pathlib
import sys
import unittest
from unittest import mock
import run_local_coverage as rlc


class RunLocalCoverageTest(unittest.TestCase):
  """Tests LLVM tool resolution, subprocess execution, profile merging,
  and main CLI workflow.
  """

  def setUp(self) -> None:
    """Sets up subprocess and LLVM tool path mocks."""
    self.run_patcher = mock.patch('subprocess.run')
    self.mock_run = self.run_patcher.start()
    self.addCleanup(self.run_patcher.stop)

    self.tool_patcher = mock.patch('run_local_coverage.find_llvm_tool')
    self.mock_tool = self.tool_patcher.start()
    self.mock_tool.return_value = pathlib.Path('/fake/bin/llvm-tool')
    self.addCleanup(self.tool_patcher.stop)

    self.check_patcher = mock.patch('subprocess.check_call')
    self.mock_check = self.check_patcher.start()
    self.addCleanup(self.check_patcher.stop)

  def test_find_llvm_tool_success(self) -> None:
    """Verifies tool resolution when executable exists."""
    self.tool_patcher.stop()
    with mock.patch.object(pathlib.Path, 'exists', return_value=True):
      res = rlc.find_llvm_tool('llvm-profdata')
      self.assertEqual(res.name, 'llvm-profdata')

  def test_find_llvm_tool_downloads_missing(self) -> None:
    """Verifies missing LLVM tool is downloaded automatically."""
    self.tool_patcher.stop()
    with mock.patch.object(pathlib.Path, 'exists', side_effect=[False, True]):
      res = rlc.find_llvm_tool('llvm-profdata')
      self.assertEqual(res.name, 'llvm-profdata')
      self.assertTrue(self.mock_check.called)

  def test_find_llvm_tool_download_failure(self) -> None:
    """Verifies sys.exit(1) when downloading LLVM tools fails."""
    self.tool_patcher.stop()
    self.mock_check.side_effect = rlc.subprocess.CalledProcessError(
        1, 'update.py')
    with mock.patch.object(pathlib.Path, 'exists', return_value=False):
      err_buf = io.StringIO()
      with contextlib.redirect_stderr(err_buf):
        with self.assertRaises(SystemExit) as cm:
          rlc.find_llvm_tool('missing-tool')
      self.assertEqual(cm.exception.code, 1)
      self.assertIn('Failed to download', err_buf.getvalue())

  def test_find_llvm_tool_not_found_after_update(self) -> None:
    """Verifies sys.exit(1) when LLVM executable is still missing
    after update.
    """
    self.tool_patcher.stop()
    with mock.patch.object(pathlib.Path, 'exists', return_value=False):
      err_buf = io.StringIO()
      with contextlib.redirect_stderr(err_buf):
        with self.assertRaises(SystemExit) as cm:
          rlc.find_llvm_tool('missing-tool')
      self.assertEqual(cm.exception.code, 1)
      self.assertIn('still not found', err_buf.getvalue())

  def test_run_test_binary_injects_env(self) -> None:
    """Verifies LLVM_PROFILE_FILE is injected into subprocess env."""
    self.mock_run.return_value = mock.MagicMock(returncode=0)
    bin_path = pathlib.Path('out/Default/net_unittests')
    prof_dir = pathlib.Path('scratch/prof')
    res = rlc.run_test_binary(bin_path, prof_dir, ['--gtest_filter=Foo'])
    self.assertEqual(res, 0)
    _, kwargs = self.mock_run.call_args
    self.assertIn('LLVM_PROFILE_FILE', kwargs['env'])

  def test_merge_profiles_no_files(self) -> None:
    """Verifies merge returns False when profraw files are missing."""
    with mock.patch.object(pathlib.Path, 'glob', return_value=[]):
      err_buf = io.StringIO()
      with contextlib.redirect_stderr(err_buf):
        res = rlc.merge_profiles(pathlib.Path('dir'),
                                 pathlib.Path('out.profdata'))
      self.assertFalse(res)
      self.assertIn('No profraw files found', err_buf.getvalue())

  def test_merge_profiles_subprocess_failure(self) -> None:
    """Verifies merge returns False when llvm-profdata fails."""
    fake_files = [pathlib.Path('a.profraw')]
    self.mock_run.return_value = mock.MagicMock(returncode=1)
    with mock.patch.object(pathlib.Path, 'glob', return_value=fake_files):
      res = rlc.merge_profiles(pathlib.Path('dir'),
                               pathlib.Path('out.profdata'))
      self.assertFalse(res)

  def test_merge_profiles_success(self) -> None:
    """Verifies merge returns True when llvm-profdata succeeds."""
    fake_files = [pathlib.Path('a.profraw')]
    self.mock_run.return_value = mock.MagicMock(returncode=0)
    with mock.patch.object(pathlib.Path, 'glob', return_value=fake_files):
      res = rlc.merge_profiles(pathlib.Path('dir'),
                               pathlib.Path('out.profdata'))
      self.assertTrue(res)

  def test_export_coverage_failure(self) -> None:
    """Verifies llvm-cov failure returns None and prints stderr."""
    mock_proc = mock.MagicMock(returncode=1, stderr='bad profile')
    self.mock_run.return_value = mock_proc
    err_buf = io.StringIO()
    with contextlib.redirect_stderr(err_buf):
      out = rlc.export_coverage(pathlib.Path('bin'), pathlib.Path('prof.data'),
                                None)
    self.assertIsNone(out)
    self.assertIn('llvm-cov failed', err_buf.getvalue())

  def test_export_coverage_success(self) -> None:
    """Verifies stdout is returned when llvm-cov succeeds."""
    mock_proc = mock.MagicMock(returncode=0, stdout='Coverage: 100%')
    self.mock_run.return_value = mock_proc
    out = rlc.export_coverage(pathlib.Path('bin'), pathlib.Path('prof.data'),
                              'foo.cc')
    self.assertEqual(out, 'Coverage: 100%')
    args, _ = self.mock_run.call_args
    self.assertIn('foo.cc', args[0])

  def test_compile_binary_success(self) -> None:
    """Verifies compile_binary executes autoninja and returns True
    on success.
    """
    self.mock_run.return_value = mock.MagicMock(returncode=0)
    bin_path = pathlib.Path('out/Coverage/net_unittests')
    res = rlc.compile_binary(bin_path)
    self.assertTrue(res)
    self.mock_run.assert_called_once_with(
        ['autoninja', '-C', 'out/Coverage', 'net_unittests'], check=False)

  def test_compile_binary_failure(self) -> None:
    """Verifies compile_binary returns False on autoninja failure."""
    self.mock_run.return_value = mock.MagicMock(returncode=1)
    bin_path = pathlib.Path('out/Coverage/net_unittests')
    res = rlc.compile_binary(bin_path)
    self.assertFalse(res)

  def test_main_success_flow(self) -> None:
    """Verifies full CLI workflow printing report to stdout."""
    test_args = ['rlc.py', '--binary', 'net_unittests']
    out_buf = io.StringIO()
    err_buf = io.StringIO()
    with (
        mock.patch.object(pathlib.Path, 'mkdir'),
        mock.patch('run_local_coverage.compile_binary', return_value=True),
        mock.patch('run_local_coverage.run_test_binary', return_value=1),
        mock.patch('run_local_coverage.merge_profiles', return_value=True),
        mock.patch('run_local_coverage.export_coverage',
                   return_value='REPORT 100%'),
        mock.patch.object(sys, 'argv', test_args),
        contextlib.redirect_stdout(out_buf),
        contextlib.redirect_stderr(err_buf),
    ):
      rlc.main()
    self.assertIn('REPORT 100%', out_buf.getvalue())
    self.assertIn('exited with 1', err_buf.getvalue())

  def test_main_compile_failure(self) -> None:
    """Verifies sys.exit(1) when compile_binary fails in main."""
    test_args = ['rlc.py', '--binary', 'net_unittests']
    err_buf = io.StringIO()
    with (
        mock.patch('run_local_coverage.compile_binary', return_value=False),
        mock.patch.object(sys, 'argv', test_args),
        contextlib.redirect_stderr(err_buf),
        self.assertRaises(SystemExit) as cm,
    ):
      rlc.main()
    self.assertEqual(cm.exception.code, 1)
    self.assertIn('Failed to compile target', err_buf.getvalue())

  def test_main_no_compile_flag(self) -> None:
    """Verifies passing --no-compile skips compile_binary."""
    test_args = ['rlc.py', '--binary', 'net_unittests', '--no-compile']
    with (
        mock.patch.object(pathlib.Path, 'mkdir'),
        mock.patch('run_local_coverage.compile_binary') as mock_compile,
        mock.patch('run_local_coverage.run_test_binary', return_value=0),
        mock.patch('run_local_coverage.merge_profiles', return_value=True),
        mock.patch('run_local_coverage.export_coverage', return_value='OK'),
        mock.patch.object(sys, 'argv', test_args),
    ):
      rlc.main()
    mock_compile.assert_not_called()

  def test_main_merge_failure(self) -> None:
    """Verifies sys.exit(1) when profile merge step fails in main."""
    test_args = ['rlc.py', '--binary', 'net_unittests']
    with (
        mock.patch.object(pathlib.Path, 'mkdir'),
        mock.patch('run_local_coverage.compile_binary', return_value=True),
        mock.patch('run_local_coverage.run_test_binary', return_value=0),
        mock.patch('run_local_coverage.merge_profiles', return_value=False),
        mock.patch.object(sys, 'argv', test_args),
        self.assertRaises(SystemExit) as cm,
    ):
      rlc.main()
    self.assertEqual(cm.exception.code, 1)


if __name__ == '__main__':
  unittest.main()
