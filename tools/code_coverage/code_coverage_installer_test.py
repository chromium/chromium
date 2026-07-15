#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for code_coverage_installer.py."""

import contextlib
import io
import pathlib
import unittest
from unittest.mock import patch

from pyfakefs import fake_filesystem_unittest

import code_coverage_installer


class CodeCoverageInstallerTest(fake_filesystem_unittest.TestCase):
  """Tests individual filesystem verification and setup helper functions."""

  def setUp(self) -> None:
    """Sets up virtual filesystem, stream redirection, and command mock."""
    self.setUpPyfakefs()
    self.src_root = pathlib.Path('/src')
    self.fs.create_dir('/src')

    self.enterContext(contextlib.redirect_stdout(io.StringIO()))
    self.enterContext(contextlib.redirect_stderr(io.StringIO()))

    self.run_command_patcher = patch('code_coverage_installer.run_command')
    self.mock_run_command = self.run_command_patcher.start()
    self.addCleanup(self.run_command_patcher.stop)

  def test_verify_chromium_checkout_success(self) -> None:
    """Verifies checkout succeeds when DEPS file exists."""
    self.fs.create_file('/src/DEPS')
    result = code_coverage_installer.verify_chromium_checkout(self.src_root)
    self.assertTrue(result)

  def test_verify_chromium_checkout_missing_deps(self) -> None:
    """Verifies checkout fails when DEPS file is missing."""
    result = code_coverage_installer.verify_chromium_checkout(self.src_root)
    self.assertFalse(result)

  def test_verify_llvm_tools_success(self) -> None:
    """Verifies LLVM tools check succeeds when both binaries exist."""
    self.fs.create_file(
        '/src/third_party/llvm-build/Release+Asserts/bin/llvm-cov')
    self.fs.create_file(
        '/src/third_party/llvm-build/Release+Asserts/bin/llvm-profdata')
    result = code_coverage_installer.verify_llvm_tools(self.src_root)
    self.assertTrue(result)

  def test_verify_llvm_tools_missing(self) -> None:
    """Verifies LLVM tools check fails when binaries are missing."""
    result = code_coverage_installer.verify_llvm_tools(self.src_root)
    self.assertFalse(result)

  def test_verify_recipes_success(self) -> None:
    """Verifies recipes check succeeds when recipe module folder exists."""
    self.fs.create_dir('/infra_dir/build/recipes/recipe_modules/code_coverage')
    infra_dir = pathlib.Path('/infra_dir')
    result = code_coverage_installer.verify_recipes(infra_dir)
    self.assertTrue(result)

  def test_verify_recipes_missing(self) -> None:
    """Verifies recipes check fails when recipe module folder is missing."""
    infra_dir = pathlib.Path('/infra_dir')
    result = code_coverage_installer.verify_recipes(infra_dir)
    self.assertFalse(result)

  def test_verify_service_success(self) -> None:
    """Verifies FindIt service check succeeds when service directory exists."""
    self.fs.create_dir('/infra_dir/infra/appengine/findit')
    infra_dir = pathlib.Path('/infra_dir')
    result = code_coverage_installer.verify_service(infra_dir)
    self.assertTrue(result)

  def test_verify_service_missing(self) -> None:
    """Verifies FindIt service check fails when service directory is missing."""
    infra_dir = pathlib.Path('/infra_dir')
    result = code_coverage_installer.verify_service(infra_dir)
    self.assertFalse(result)

  def test_setup_infra_gclient_exists_sync_success(self) -> None:
    """Verifies running gclient sync when .gclient already exists."""
    infra_dir = pathlib.Path('/infra_dir')
    self.fs.create_file('/infra_dir/.gclient')
    self.mock_run_command.return_value = 0
    result = code_coverage_installer.setup_infra(infra_dir)
    self.assertTrue(result)
    self.mock_run_command.assert_called_once_with(['gclient', 'sync'],
                                                  cwd=infra_dir)

  def test_setup_infra_gclient_exists_sync_failure(self) -> None:
    """Verifies setup fails when gclient sync returns non-zero code."""
    infra_dir = pathlib.Path('/infra_dir')
    self.fs.create_file('/infra_dir/.gclient')
    self.mock_run_command.return_value = 1
    result = code_coverage_installer.setup_infra(infra_dir)
    self.assertFalse(result)

  def test_setup_infra_no_gclient_fetch_success(self) -> None:
    """Verifies fetch infra_superproject runs when .gclient is missing."""
    infra_dir = pathlib.Path('/infra_dir')
    self.mock_run_command.return_value = 0
    result = code_coverage_installer.setup_infra(infra_dir)
    self.assertTrue(result)
    self.mock_run_command.assert_called_once_with(
        ['fetch', 'infra_superproject'], cwd=infra_dir)

  def test_setup_infra_no_gclient_fetch_failure(self) -> None:
    """Verifies setup fails when fetch infra_superproject returns non-zero."""
    infra_dir = pathlib.Path('/infra_dir')
    self.mock_run_command.return_value = 1
    result = code_coverage_installer.setup_infra(infra_dir)
    self.assertFalse(result)

  def test_fs_permission_error_handled(self) -> None:
    """Verifies filesystem permission errors are caught gracefully."""
    with patch('pathlib.Path.exists',
               side_effect=PermissionError('Access Denied')):
      result = code_coverage_installer.verify_chromium_checkout(self.src_root)
    self.assertFalse(result)

  def test_verify_gemini_md_creates_new(self) -> None:
    """Verifies creating GEMINI.md with template import when file missing."""
    result = code_coverage_installer.verify_gemini_md(self.src_root)
    self.assertTrue(result)
    content = (self.src_root / 'GEMINI.md').read_text(encoding='utf-8')
    self.assertIn('@agents/prompts/templates/code_coverage.md', content)

  def test_verify_gemini_md_appends_existing(self) -> None:
    """Verifies appending template import when GEMINI.md already exists."""
    gemini_path = self.src_root / 'GEMINI.md'
    gemini_path.write_text('# Existing prompt\n', encoding='utf-8')
    result = code_coverage_installer.verify_gemini_md(self.src_root)
    self.assertTrue(result)
    content = gemini_path.read_text(encoding='utf-8')
    self.assertIn('# Existing prompt', content)
    self.assertIn('@agents/prompts/templates/code_coverage.md', content)

  def test_verify_gemini_md_already_present(self) -> None:
    """Verifies no duplicate entry added when import is already present."""
    gemini_path = self.src_root / 'GEMINI.md'
    gemini_path.write_text('@agents/prompts/templates/code_coverage.md\n',
                           encoding='utf-8')
    result = code_coverage_installer.verify_gemini_md(self.src_root)
    self.assertTrue(result)


class CodeCoverageInstallerMainTest(unittest.TestCase):
  """Tests full CLI main() workflow with mocked environment verification."""

  def setUp(self) -> None:
    """Initializes all verification patchers to isolate main() CLI execution."""
    self.enterContext(contextlib.redirect_stdout(io.StringIO()))
    self.enterContext(contextlib.redirect_stderr(io.StringIO()))

    self.patcher_argv = patch(
        'sys.argv', ['code_coverage_installer.py', '--infra-dir', '/infra_dir'])
    self.patcher_exit = patch('sys.exit', side_effect=SystemExit)
    self.patcher_gemini = patch('code_coverage_installer.verify_gemini_md',
                                return_value=True)
    self.patcher_checkout = patch(
        'code_coverage_installer.verify_chromium_checkout')
    self.patcher_llvm = patch('code_coverage_installer.verify_llvm_tools')
    self.patcher_recipes = patch('code_coverage_installer.verify_recipes')
    self.patcher_service = patch('code_coverage_installer.verify_service')
    self.patcher_setup = patch('code_coverage_installer.setup_infra')

    self.mock_checkout = self.patcher_checkout.start()
    self.mock_llvm = self.patcher_llvm.start()
    self.mock_recipes = self.patcher_recipes.start()
    self.mock_service = self.patcher_service.start()
    self.mock_gemini = self.patcher_gemini.start()
    self.mock_setup = self.patcher_setup.start()
    self.mock_exit = self.patcher_exit.start()
    self.patcher_argv.start()

    self.addCleanup(patch.stopall)

  def test_main_checkout_failed(self) -> None:
    """Verifies main() exits with status 1 if Chromium checkout check fails."""
    self.mock_checkout.return_value = False
    with self.assertRaises(SystemExit):
      code_coverage_installer.main()
    self.mock_exit.assert_called_once_with(1)

  def test_main_llvm_failed(self) -> None:
    """Verifies main() exits with status 1 if LLVM tools check fails."""
    self.mock_checkout.return_value = True
    self.mock_llvm.return_value = False
    with self.assertRaises(SystemExit):
      code_coverage_installer.main()
    self.mock_exit.assert_called_once_with(1)

  def test_main_already_fully_set_up(self) -> None:
    """Verifies main() succeeds immediately without setup when checks pass."""
    self.mock_checkout.return_value = True
    self.mock_llvm.return_value = True
    self.mock_recipes.return_value = True
    self.mock_service.return_value = True
    code_coverage_installer.main()
    self.assertEqual(self.mock_exit.call_count, 0)

  def test_main_setup_needed_and_succeeds(self) -> None:
    """Verifies main() runs setup_infra when recipes/service check fails."""
    self.mock_checkout.return_value = True
    self.mock_llvm.return_value = True
    self.mock_recipes.side_effect = [False, True]
    self.mock_service.side_effect = [False, True]
    self.mock_setup.return_value = True
    code_coverage_installer.main()
    self.mock_setup.assert_called_once_with(pathlib.Path('/infra_dir'))
    self.assertEqual(self.mock_exit.call_count, 0)

  def test_main_setup_needed_but_setup_fails(self) -> None:
    """Verifies main() exits with status 1 when setup_infra fails."""
    self.mock_checkout.return_value = True
    self.mock_llvm.return_value = True
    self.mock_recipes.return_value = False
    self.mock_service.return_value = False
    self.mock_setup.return_value = False
    with self.assertRaises(SystemExit):
      code_coverage_installer.main()
    self.mock_setup.assert_called_once_with(pathlib.Path('/infra_dir'))
    self.mock_exit.assert_called_once_with(1)

  def test_main_setup_succeeds_but_re_verification_fails(self) -> None:
    """Verifies main() exits with status 1 if re-verification fails."""
    self.mock_checkout.return_value = True
    self.mock_llvm.return_value = True
    self.mock_recipes.side_effect = [False, False]
    self.mock_service.side_effect = [False, True]
    self.mock_setup.return_value = True
    with self.assertRaises(SystemExit):
      code_coverage_installer.main()
    self.mock_setup.assert_called_once_with(pathlib.Path('/infra_dir'))
    self.mock_exit.assert_called_once_with(1)


if __name__ == '__main__':
  unittest.main()
