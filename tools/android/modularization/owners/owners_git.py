# Lint as: python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Git utility functions.'''

import os
import subprocess
import sys

from typing import List, Optional


def get_head_hash(git_src: str) -> str:
  '''Gets the repository's head hash.'''
  return run_command(['git', 'rev-parse', 'HEAD'], cwd=git_src)


def get_last_commit_date(git_src: str) -> str:
  '''Gets the repository's time of last commit.'''
  return run_command(['git', 'log', '-1', '--format=%ct'], cwd=git_src)


def get_total_lines_of_code(git_src: str, subdirectory: str) -> int:
  '''Gets the number of lines contained in the git directory.'''
  filepaths = _run_ls_files_command(subdirectory, git_src)

  total_loc = 0
  for filepath in filepaths:
    with open(filepath, 'rb') as f:
      total_loc += sum(1 for line in f)

  return total_loc


def get_total_files(git_src: str, subdirectory: str) -> int:
  '''Gets the number of files contained in the git directory.'''
  filepaths = _run_ls_files_command(subdirectory, git_src)
  return len(filepaths)


def _run_ls_files_command(subdirectory: Optional[str],
                          git_src: str) -> List[str]:
  command = _build_ls_files_command(subdirectory)
  filepath_str = run_command(command, cwd=git_src)
  result = []
  for relative_filepath in filepath_str.split('\n'):
    if relative_filepath:
      absolute_filepath = os.path.join(git_src, relative_filepath)
      result.append(absolute_filepath)
  return result


def _build_ls_files_command(subdirectory: Optional[str]) -> List[str]:
  if subdirectory:
    return ['git', 'ls-files', '--', subdirectory]
  else:
    return ['git', 'ls-files']


def get_log(git_src: str, subdirectory: str, trailing_days: int) -> str:
  '''Gets the git log for a given directory.'''
  return run_command([
      'git',
      'log',
      '--follow',
      f'--since=\"{trailing_days} days ago\"',
      '--',
      subdirectory,
  ],
                     cwd=git_src)


def run_command(command: List[str], cwd: str) -> str:
  '''Runs a command and returns the output.

    Raises an exception and prints the command output if the command fails.'''
  try:
    run_result = subprocess.run(command,
                                capture_output=True,
                                text=True,
                                check=True,
                                cwd=cwd)
  except subprocess.CalledProcessError as e:
    print(f'{command} failed with code {e.returncode}.', file=sys.stderr)
    print(f'\nSTDERR:\n{e.stderr}', file=sys.stderr)
    print(f'\nSTDOUT:\n{e.stdout}', file=sys.stderr)
    raise
  return run_result.stdout.strip()
