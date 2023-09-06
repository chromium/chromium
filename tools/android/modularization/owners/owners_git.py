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
  for l in filepath_str.split('\n'):
    # git ls-files -s produces output in the format:
    #
    # [mode bits] [hash]            [merge stage] [file path]
    # 100644      0123456789abcdef  0             chrome/browser/Foo.java
    #
    # The first three octal numbers of |mode bits| are '100' for files, and
    # checking that allows skipping gitlinks ('160') and symlinks ('120').
    if not l.startswith('100'):
      # ls-files returns all git files, such as files and gitlinks. Return only
      # files, which start with 100.
      continue
    relative_filepath = l.split(maxsplit=3)[-1]
    if relative_filepath:
      absolute_filepath = os.path.join(git_src, relative_filepath)
      result.append(absolute_filepath)
  return result


def _build_ls_files_command(subdirectory: Optional[str]) -> List[str]:
  if subdirectory:
    return ['git', 'ls-files', '-s', '--', subdirectory]
  else:
    return ['git', 'ls-files', '-s']


def _get_last_commit_in_dir(git_src: str, subdirectory: str,
                            trailing_days: int):
  '''Returns the last commit hash for a given directory.'''
  return run_command([
      'git', 'log', '-1', f'--since=\"{trailing_days} days ago\"',
      '--pretty=format:%H', '--', subdirectory
  ],
                     cwd=git_src)


def get_log(git_src: str, subdirectory: str, trailing_days: int, follow: bool,
            cache_dir: Optional[str]) -> str:
  '''Gets the git log for a given directory.'''
  if cache_dir is not None:
    key = subdirectory.replace(os.sep, '_')
    cache_file_name = os.path.join(cache_dir, key)
    cache_log_file_name = cache_file_name + '.log'
    last_commit = _get_last_commit_in_dir(git_src, subdirectory, trailing_days)
    if os.path.exists(cache_file_name):
      with open(cache_file_name) as f:
        cached_commit = f.read().strip()
      # Cache hit.
      if cached_commit == last_commit and os.path.exists(cache_log_file_name):
        with open(cache_log_file_name) as f:
          return f.read()

  cmd = [
      'git',
      'log',
  ]
  if follow:
    cmd.append('--follow')
  cmd.extend([
      f'--since=\"{trailing_days} days ago\"',
      '--',
      subdirectory,
  ])
  git_log_output = run_command(cmd, cwd=git_src)

  # No cache hit, need to update cache.
  if cache_dir is not None:
    with open(cache_file_name, 'w') as f:
      f.write(last_commit)
    with open(cache_log_file_name, 'w') as f:
      f.write(git_log_output)

  return git_log_output


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
