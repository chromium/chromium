# Lint as: python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper module for retrieving git repository metadata."""

import datetime as dt
import functools
import os
import pathlib
import sys
from typing import Optional, Union

_PYTHON_UTILS_PATH = pathlib.Path(__file__).resolve().parents[0]
if str(_PYTHON_UTILS_PATH) not in sys.path:
    sys.path.append(str(_PYTHON_UTILS_PATH))
import subprocess_utils

PathStr = Union[pathlib.Path, str]


@functools.lru_cache(maxsize=1)
def get_chromium_src_path() -> pathlib.Path:
    """Returns the root 'src' absolute path of this Chromium checkout.

    Example Path: /home/username/git/chromium/src

    Returns:
        The absolute path to the 'src' root directory of the Chromium checkout
        containing this file.
    """
    _CHROMIUM_SRC_ROOT = pathlib.Path(__file__).resolve(strict=True).parents[3]

    # .git directory does not exist on cog.
    if os.getcwd().startswith('/google/cog/cloud'):
        return _CHROMIUM_SRC_ROOT

    try:
        _assert_git_repository(_CHROMIUM_SRC_ROOT)
    except (ValueError, RuntimeError):
        raise AssertionError

    return _CHROMIUM_SRC_ROOT


def get_head_commit_format(git_repo: Optional[PathStr] = None,
                           format: str = '') -> str:
    """Gets formatted info from the commit at HEAD for a Git repository.

    Args:
        git_repo:
            The path to a Git repository's root directory; if not specified,
            defaults to the Chromium Git repository.
        format:
            The format string to pass to --pretty=format:<format>

    Returns:
        The output from git show with the specified format.

    Raises
        ValueError:
            The path specified in the git_repo parameter is not a root
            directory for a Git repository.
        RuntimeError:
            The path specified in the git_repo parameter contains an infinite
            loop.
    """
    if not git_repo:
        git_repo = get_chromium_src_path()

    if not isinstance(git_repo, pathlib.Path):
        git_repo = pathlib.Path(git_repo)

    _assert_git_repository(git_repo)

    return subprocess_utils.run_command(
        ['git', 'show', '--no-patch', f'--pretty=format:{format}'],
        cwd=git_repo)


def get_head_commit_hash(git_repo: Optional[PathStr] = None) -> str:
    """Gets the hash of the commit at HEAD for a Git repository.

    This returns the full, non-abbreviated, SHA1 hash of the commit as a string
    containing 40 hexadecimal characters. For example,
    '632918ad686949a9bc5f17ee1b48fa48e81be645'.
    """
    return get_head_commit_format(git_repo, '%H')


def get_head_commit_time(git_repo: Optional[PathStr] = None) -> str:
    """Gets the time of the commit at HEAD for a Git repo in string form."""
    return get_head_commit_format(git_repo, '%cd')


def get_head_commit_datetime(git_repo: Optional[PathStr] = None
                             ) -> dt.datetime:
    """Gets the datetime of the commit at HEAD for a Git repository in UTC.

    The datetime returned contains timezone information (in timezone.utc) so
    that it can be easily be formatted or converted (e.g., to local time) based
    on the caller's needs.
    """
    timestamp = get_head_commit_format(git_repo, '%ct')
    return dt.datetime.fromtimestamp(float(timestamp), tz=dt.timezone.utc)


def get_head_commit_cr_position(git_repo: Optional[PathStr] = None) -> str:
    """Get the cr position of the commit at HEAD for a Git repository.

    CL descriptions are typically of the form:
        '[lines...]Cr-Commit-Position: refs/heads/main@{#123456}'

    Return the string '123456' in this case. In the absence of this value from
    the CL description, return an empty string.
    """
    description: str = get_head_commit_format(git_repo, '%b')
    # Will capture from
    #  the string
    # '123456'.
    # Examine lines from the description in reverse order since for reverts, we
    # want to match the last Cr-Commit-Position value.
    for line in reversed(description.splitlines()):
        if 'Cr-Commit-Position: ' in line:
            last_hash_idx = line.rfind('#')
            assert last_hash_idx != -1, (
                f'Could not find # in Cr-Commit-Position line: {line}.')
            last_right_curly_idx = line.rfind('}')
            assert last_hash_idx < last_right_curly_idx, (
                'Could not find } after # in ' + line)
            return line[last_hash_idx + 1:last_right_curly_idx]
    return ''


def _assert_git_repository(git_repo_root: pathlib.Path) -> None:
    try:
        repo_path = git_repo_root.resolve(strict=True)
    except FileNotFoundError as err:
        raise ValueError(
            f'The Git repository root "{git_repo_root}" is invalid;'
            f' {err.strerror}: "{err.filename}".')

    if not repo_path.is_dir():
        raise ValueError(
            f'The Git repository root "{git_repo_root}" is invalid;'
            f' not a directory.')

    try:
        git_internals_path = repo_path.joinpath('.git').resolve(strict=True)
    except FileNotFoundError as err:
        raise ValueError(
            f'The path "{git_repo_root}" is not a root directory for a Git'
            f' repository; {err.strerror}: "{err.filename}".')

    if not repo_path.is_dir():
        raise ValueError(
            f'The Git repository root "{git_repo_root}" is invalid;'
            f' {git_internals_path} is not a directory.')
