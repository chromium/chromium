#!/usr/bin/env python3

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Sets up jj for a Chromium checkout."""

import logging
import os
import pathlib
import shutil
import subprocess
import sys

import util


def main():
  script_dir = pathlib.Path(__file__).resolve().parent
  repo_root = script_dir.parent.parent
  os.chdir(repo_root)

  if not (repo_root / '.jj').is_dir():
    util.run_jj(['git', 'init', '--colocate', '.'])

  # Link the shared jj config into the repo.
  config_path = pathlib.Path(
      util.run_jj(['config', 'path', '--repo'],
                  stdout=subprocess.PIPE,
                  text=True).stdout.strip())
  config_path.unlink(missing_ok=True)
  config_source = (script_dir / 'config.toml').resolve()
  try:
    config_path.symlink_to(config_source)
  except OSError:
    shutil.copy2(str(config_source), str(config_path))
    logging.warning(
        'Could not create symlink; copied config.toml instead. Future changes '
        'to tools/jj/config.toml will need to be re-copied manually.')
    if sys.platform == 'win32':
      logging.warning(
          'On Windows, symlinks require Developer Mode to be enabled or '
          'administrator privileges.')

  # Ensure that jj snapshots the current commit so it doesn't get lost
  # with git switch.
  util.run_jj(['new'])

  # Fix issues with line endings. See go/jj-in-chromium.
  util.run_command(['git', 'config', 'core.autocrlf', 'false'])
  util.run_command(['git', 'switch', 'origin/main', '--detach'])
  util.run_jj(['abandon'])
  util.run_command(['git', 'add', '-A'])

  print('Reminder: If you haven\'t already, we recommend joining '
        'https://groups.google.com/g/chromium-jj-users')


if __name__ == '__main__':
  main()
