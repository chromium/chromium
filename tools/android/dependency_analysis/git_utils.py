# Lint as: python3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper module for retrieving git metadata."""

import re

import subprocess_utils


def get_last_commit_hash() -> str:
    """Get the git hash of the last git commit to the git repo.

    The cwd must be a git repository.
    """
    return _get_last_commit_with_format('%H')


def get_last_commit_time() -> str:
    """Get the commit date/time of the last git commit to the git repo.

    The cwd must be a git repository.
    """
    return _get_last_commit_with_format('%cd')


def get_last_commit_cr_position() -> str:
    """Get the cr position of the last git commit to the git repo.

    This is the number that follows "Cr-Commit-Position:". In the absence of
    this value, returns an empty string.

    The cwd must be a chromium git repository.
    """
    description: str = _get_last_commit_with_format('%b')
    # Will capture from
    # '[lines...]Cr-Commit-Position: refs/heads/master@{#123456}' the string
    # '123456'.
    CR_POSITION_REGEX = r'Cr-Commit-Position: .*{#([0-9]+)}'
    match: re.Match = re.search(CR_POSITION_REGEX, description)
    if match is None:
        return ''
    return match.group(1)


def _get_last_commit_with_format(format: str) -> str:
    output_str: str = subprocess_utils.run_command(
        ['git', 'show', '--no-patch', f'--pretty=format:{format}'])
    return output_str.strip('\n')
