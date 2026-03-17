# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import unittest
from unittest.mock import patch, MagicMock, mock_open
from pathlib import Path
import sys
import os

# Mocking sys.exit to avoid test runner exit
# Importing worktree
import worktree


class TestWorktree(unittest.TestCase):

  @patch("worktree.subprocess.run")
  def test_run_cmd(self, mock_run):
    worktree.run_cmd(["echo", "hi"])
    mock_run.assert_called_once_with(["echo", "hi"],
                                     cwd=None,
                                     check=True,
                                     text=True,
                                     capture_output=False)

  @patch("worktree.subprocess.run")
  def test_run_cmd_no_check_failure(self, mock_run):
    import subprocess
    mock_run.side_effect = subprocess.CalledProcessError(1, ["false"])
    result = worktree.run_cmd(["false"], check=False)
    self.assertIsNone(result)

  @patch("pathlib.Path.is_dir", return_value=True)
  @patch("pathlib.Path.exists", side_effect=[True, True, True])
  @patch("pathlib.Path.resolve")
  def test_validate_parent_repo_success(self, mock_resolve, mock_exists,
                                        mock_is_dir):
    mock_resolve.return_value = Path("/fake/repo")
    parent_dir, g_config, s_dir = worktree.validate_parent_repo("/fake/repo")
    self.assertEqual(parent_dir, Path("/fake/repo"))
    self.assertEqual(g_config, Path("/fake/repo/.gclient"))
    self.assertEqual(s_dir, Path("/fake/repo/src"))

  @patch("pathlib.Path.is_dir", return_value=True)
  @patch("pathlib.Path.exists", side_effect=[True, True, True])
  @patch("pathlib.Path.resolve")
  def test_validate_parent_repo_from_src(self, mock_resolve, mock_exists,
                                         mock_is_dir):
    # Simulate being called from /fake/repo/src
    mock_resolve.return_value = Path("/fake/repo/src")
    parent_dir, g_config, s_dir = worktree.validate_parent_repo(
        "/fake/repo/src")
    self.assertEqual(parent_dir, Path("/fake/repo"))
    self.assertEqual(g_config, Path("/fake/repo/.gclient"))
    self.assertEqual(s_dir, Path("/fake/repo/src"))

  @patch("pathlib.Path.read_text",
         return_value='solutions = []\ncache_dir = "/tmp/cache"')
  def test_check_cache_dir_present(self, mock_read):
    with patch("builtins.print") as mock_print:
      worktree.check_cache_dir(Path("fake/.gclient"))
      # Should not print warning
      self.assertFalse(
          any("WARNING" in str(call) for call in mock_print.call_args_list))

  @patch("pathlib.Path.read_text", return_value='solutions = []')
  def test_check_cache_dir_missing(self, mock_read):
    with patch("builtins.print") as mock_print:
      worktree.check_cache_dir(Path("fake/.gclient"))
      # Should print warning
      mock_print.assert_any_call(
          "\nWARNING: 'cache_dir' not found in your .gclient file.")

  @patch("worktree.run_cmd")
  def test_create_worktree(self, mock_run):
    worktree.create_worktree("parent/src", "variant/src", "my_branch")
    mock_run.assert_called_with(
        ["git", "worktree", "add", "variant/src", "-b", "my_branch"],
        cwd="parent/src")

  @patch("worktree.run_cmd")
  def test_create_worktree_with_base(self, mock_run):
    worktree.create_worktree("parent/src", "variant/src", "my_branch",
                             "base_branch")
    # Should call git worktree add AND git branch --set-upstream-to
    self.assertEqual(mock_run.call_count, 2)
    mock_run.assert_any_call([
        "git", "worktree", "add", "variant/src", "-b", "my_branch",
        "base_branch"
    ],
                             cwd="parent/src")
    mock_run.assert_any_call(
        ["git", "branch", "--set-upstream-to", "base_branch", "my_branch"],
        cwd="variant/src")

  def test_setup_vscode_color(self):
    with patch("pathlib.Path.mkdir"), \
         patch("pathlib.Path.exists", return_value=False), \
         patch("pathlib.Path.write_text") as mock_write:

      variant_src = Path("/tmp/src")
      worktree.setup_vscode_color(variant_src, "red")

      # Check if it writes valid JSON with titleBar colors
      args, _ = mock_write.call_args
      content = json.loads(args[0])
      self.assertEqual(
          content["workbench.colorCustomizations"]["titleBar.activeBackground"],
          "#3a1414")

  @patch("worktree.Path.exists")
  def test_get_next_variant_name(self, mock_exists):
    # Simulate 'bling_1' exists (True), but 'bling_2' does not (False)
    mock_exists.side_effect = [True, False]
    parent_dir = Path("/Users/vincb/bling")

    name = worktree.get_next_variant_name(parent_dir)
    self.assertEqual(name, "bling_2")

  @patch("worktree.validate_parent_repo")
  @patch("worktree.check_cache_dir")
  @patch("worktree.Path.mkdir")
  @patch("worktree.create_worktree")
  @patch("shutil.copy2")
  @patch("worktree.run_cmd")
  @patch("worktree.Path.exists")
  def test_main_internal_base(self, mock_exists, mock_run, mock_copy,
                              mock_create, mock_mkdir, mock_check,
                              mock_validate):
    # Mocking validate_parent_repo
    parent_dir = Path("/fake/repo")
    gclient_config = parent_dir / ".gclient"
    parent_src = parent_dir / "src"
    mock_validate.return_value = (parent_dir, gclient_config, parent_src)
    mock_check.return_value = True

    # Mocking variant setup
    # 1. get_next_variant_name (will be called in main)
    # 3. internal_src.exists() (will be True)
    mock_exists.side_effect = [False, False, True]

    # Mocking args
    with patch("sys.argv", [
        "worktree.py", "/fake/repo", "--internal-base", "internal_base_branch"
    ]):
      worktree.main()

      # Verify internal logic
      # run_cmd should be called for:
      # 1. gclient sync
      # 2. git checkout -b ... (internal)
      # 3. git branch --set-upstream-to ... (internal)

      # Find the index of internal commands in mock_run.call_args_list
      calls = [call.args[0] for call in mock_run.call_args_list]
      self.assertIn(["git", "checkout", "-b", "repo_1", "internal_base_branch"],
                    calls)
      self.assertIn([
          "git", "branch", "--set-upstream-to", "internal_base_branch", "repo_1"
      ], calls)

    @patch("worktree.validate_parent_repo")
    @patch("worktree.check_cache_dir")
    @patch("worktree.Path.mkdir")
    @patch("worktree.create_worktree")
    @patch("shutil.copy2")
    @patch("worktree.run_cmd")
    @patch("worktree.Path.exists")
    def test_main_internal_base_fetch_from_parent(self, mock_exists, mock_run,
                                                  mock_copy, mock_create,
                                                  mock_mkdir, mock_check,
                                                  mock_validate):
      # Mocking validate_parent_repo
      parent_dir = Path("/fake/repo")
      gclient_config = parent_dir / ".gclient"
      parent_src = parent_dir / "src"
      mock_validate.return_value = (parent_dir, gclient_config, parent_src)
      mock_check.return_value = True

      # Mocking variant setup
      # 1. get_next_variant_name (will be called in main)
      # 2. variant_root.exists() (will be False)
      # 3. internal_src.exists() (will be True)
      mock_exists.side_effect = [False, False, True]

      # Mocking run_cmd:
      # 1. validate_parent_repo might call run_cmd (it doesn't in our mocks)
      # 2. git show-ref --verify refs/heads/internal_base_branch (return None to simulate missing)
      # 3. git remote add parent ...
      # 4. git fetch parent ...
      # 5. git checkout -b ...
      # 6. git branch --set-upstream-to ...
      mock_run.side_effect = [
          None,  # git show-ref returns None (captured=True)
          None,  # git remote add
          None,  # git fetch
          None,  # git checkout
          None  # git branch --set-upstream-to
      ]

      # Mocking args
      with patch("sys.argv", [
          "worktree.py", "/fake/repo", "--internal-base", "internal_base_branch"
      ]):
        worktree.main()

        # Verify ios_internal logic
        calls = [call.args[0] for call in mock_run.call_args_list]
        self.assertIn(
            ["git", "remote", "add", "parent",
             str(parent_src / "internal")], calls)
        self.assertIn(["git", "fetch", "parent", "internal_base_branch"], calls)
        self.assertIn(["git", "checkout", "-b", "repo_1", "FETCH_HEAD"], calls)
        self.assertIn([
            "git", "branch", "--set-upstream-to", "parent/internal_base_branch",
            "repo_1"
        ], calls)


if __name__ == "__main__":
  unittest.main()
