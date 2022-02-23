# Lint as: python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper module for retrieving git repository metadata."""

import datetime as dt
import functools
import pathlib
from typing import Optional, Union

from . import subprocess_utils


@functools.lru_cache(maxsize=1)
def get_chromium_src_path() -> pathlib.Path:
    """Returns the root 'src' absolute path of this Chromium Git checkout.

    Example Path: /home/username/git/chromium/src

    Returns:
        The absolute path to the 'src' root directory of the Chromium Git
        checkout containing this file.
    """
    _CHROMIUM_SRC_ROOT = pathlib.Path(__file__).parents[3].resolve(strict=True)
    if _CHROMIUM_SRC_ROOT.name != 'src':
        raise AssertionError(
            f'_CHROMIUM_SRC_ROOT "{_CHROMIUM_SRC_ROOT}" should end in "src".')

    try:
        _assert_git_repository(_CHROMIUM_SRC_ROOT)
    except (ValueError, RuntimeError):
        raise AssertionError

    return _CHROMIUM_SRC_ROOT


def get_head_commit_hash(git_repo: Optional[Union[str, pathlib.Path]] = None
                         ) -> str:
    """Gets the hash of the commit at HEAD for a Git repository.

    This returns the full, non-abbreviated, SHA1 hash of the commit as a string
    containing 40 hexadecimal characters. For example,
    '632918ad686949a9bc5f17ee1b48fa48e81be645'.

    Args:
        git_repo:
            The path to a Git repository's root directory; if not specified,
            defaults to the Chromium Git repository.

    Returns:
        The SHA1 hash of the Git repository's commit at HEAD.

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
        ['git', 'show', '--no-patch', f'--pretty=format:%H'], cwd=git_repo)


def get_head_commit_datetime(
        git_repo: Optional[Union[str, pathlib.Path]] = None) -> dt.datetime:
    """Gets the datetime of the commit at HEAD for a Git repository in UTC.

    The datetime returned contains timezone information (in timezone.utc) so
    that it can be easily be formatted or converted (e.g., to local time) based
    on the caller's needs.

    Args:
        git_repo:
            The path to a Git repository's root directory; if not specified,
            defaults to the Chromium Git repository.

    Returns:
        The datetime of the Git repository's commit at HEAD.

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

    timestamp = subprocess_utils.run_command(
        ['git', 'show', '--no-patch', '--format=%ct'], cwd=git_repo)

    return dt.datetime.fromtimestamp(float(timestamp), tz=dt.timezone.utc)


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
