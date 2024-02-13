# Lint as: python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import dataclasses
import datetime as dt
import functools
import logging
import multiprocessing
import os
import pathlib
import sys
from typing import List, Optional, Set, Tuple, Union

_TOOLS_ANDROID_PATH = pathlib.Path(__file__).resolve(strict=True).parents[1]
if str(_TOOLS_ANDROID_PATH) not in sys.path:
    sys.path.append(str(_TOOLS_ANDROID_PATH))
from python_utils import git_metadata_utils

import java_test_utils

_CHROMIUM_SRC_PATH = git_metadata_utils.get_chromium_src_path()

_IGNORED_DIRS = ('out', 'third_party', 'clank', 'build/linux', 'native_client',
                 'tools/android/test_health/testdata')

_IGNORED_FILES = set()


@dataclasses.dataclass(frozen=True)
class GitRepoInfo:
    """Holder class for Git repository information."""

    git_head: str
    """The SHA1 hash of the Git repository's commit at HEAD."""

    git_head_time: dt.datetime
    """The datetime of the Git repository's commit at HEAD."""


@dataclasses.dataclass(frozen=True)
class TestHealthInfo:
    """Holder class for test health information about a test class."""

    test_name: str
    """The name of the test, e.g., the class name of a Java test."""

    test_dir: pathlib.Path
    """The directory containing the test, relative to the Git repo root."""

    test_filename: str
    """The filename of the test, e.g., FooJavaTest.java."""

    java_test_health: Optional[java_test_utils.JavaTestHealth]
    """Java test health info and counters; this is None if not a Java test."""

    git_repo_info: GitRepoInfo
    """Information about the Git repository being sampled."""


def get_repo_test_health(git_repo: Optional[pathlib.Path] = None,
                         *,
                         test_dir: Union[str, pathlib.Path, None] = None,
                         ignored_dirs: Tuple[str, ...] = _IGNORED_DIRS,
                         ignored_files: Set[str] = _IGNORED_FILES
                         ) -> List[TestHealthInfo]:
    """Gets test health information and stats for a Git repository.

    This function checks for Java tests annotated as disabled but could
    be extended to check other metrics or languages in the future.

    Args:
        git_repo:
            The path to the root of the Git repository being checked; defaults
            to the Chromium repo.
        test_dir:
            The subdirectory, relative to the Git repo root, containing the
            tests of interest; defaults to the root of the Git repo.
        ignored_dirs:
            A list of directories to skip (paths relative to `test_dir`);
            defaults to a set of directories that should be ignored in the
            Chromium Git repo.
        ignored_files:
            A set of file paths to skip (relative to `test_dir`); defaults to
            files in the Chromium Git repo with unsupported Java syntax.
    Returns:
        A list of `TestHealthInfo` objects, one for each test file processed.
    """
    git_repo = git_repo or _CHROMIUM_SRC_PATH
    test_dir = test_dir or pathlib.Path('.')
    tests_root = (git_repo / test_dir).resolve(strict=True)
    repo_info = _get_git_repo_info(git_repo)

    logging.debug(f'Starting os.walk in {tests_root}')
    test_paths = []
    for dirpath, _, filenames in os.walk(tests_root):
        if os.path.relpath(dirpath, tests_root).startswith(ignored_dirs):
            continue

        for filename in filenames:
            if not filename.endswith('Test.java'):
                continue

            test_path = pathlib.Path(dirpath) / filename
            if os.path.relpath(test_path, tests_root) in ignored_files:
                continue

            test_paths.append(test_path)

    logging.debug(f'Parsing {len(test_paths)} test files')
    with multiprocessing.Pool() as p:
        test_health_infos: list[Optional[TestHealthInfo]] = p.map(
            functools.partial(_get_test_health_info, git_repo, repo_info),
            test_paths)

    return [t for t in test_health_infos if t is not None]


def _get_test_health_info(repo_root: pathlib.Path, repo_info: GitRepoInfo,
                          test_path: pathlib.Path) -> Optional[TestHealthInfo]:
    test_file = test_path.relative_to(repo_root)
    try:
        test_health_stats = java_test_utils.get_java_test_health(test_path)
    except java_test_utils.JavaSyntaxError as e:
        # This can occur if the file uses syntax not supported by the underlying
        # javalang python module used by java_test_utils. These files should be
        # investigated manually.
        logging.warning(f'Skipped file "{test_file}" due to'
                        ' Java syntax error:')
        logging.warning(f'    {e}')
        logging.warning(f'        {e.lineno}:{e.offset}: {e.text}')
        return None

    return TestHealthInfo(test_name=test_file.stem,
                          test_dir=test_file.parent,
                          test_filename=test_file.name,
                          java_test_health=test_health_stats,
                          git_repo_info=repo_info)


def _get_git_repo_info(git_repo: pathlib.Path) -> GitRepoInfo:
    return GitRepoInfo(git_metadata_utils.get_head_commit_hash(git_repo),
                       git_metadata_utils.get_head_commit_datetime(git_repo))
