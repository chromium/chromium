# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
from pathlib import Path
from typing import Iterable, List

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
_CHROMIUM_SRC_DIR = os.path.dirname(os.path.dirname(os.path.dirname(_THIS_DIR)))

# Paths relative to src/ that we want to run the test from
_TEST_DIRECTORIES_TO_SCAN = [
    'tools/metrics', 'tools/variations', 'tools/json_comment_eater',
    'tools/json_to_struct'
]

# Lists a directories that contain test files relevant for Chrome Metrics
# tooling as path relative to src/.
TEST_DIRECTORIES_RELATIVE_TO_SRC = [
    os.path.join(_CHROMIUM_SRC_DIR, d) for d in _TEST_DIRECTORIES_TO_SCAN
]

_EXPECTED_TEST_FILES_SUFFIXES = ['test.py', 'tests.py']


def _is_test_file(file_path: str) -> bool:
  return any(
      file_path.endswith(suffix) for suffix in _EXPECTED_TEST_FILES_SUFFIXES)


def _find_all_tests_in(directory_path: str) -> Iterable[str]:
  for dir, _, files in os.walk(directory_path):
    for file in files:
      if _is_test_file(file):
        yield os.path.join(dir, file)


def find_all_tests() -> Iterable[str]:
  """Finds all python tests in all directories listed in DIRECTORIES_TO_SCAN"""
  all_test_files: List[str] = []
  for dir in TEST_DIRECTORIES_RELATIVE_TO_SRC:
    all_test_files.extend(_find_all_tests_in(dir))
  return all_test_files


def validate_gn_sources(target_group: str) -> set[str]:
  """Validates that all test files are listed in target_group of BUILD.gn

  Returns: A set of paths missing in BUILD.gn, relative to src/.
  """
  dir_path = Path(_THIS_DIR).parent
  gn_path = dir_path.joinpath("BUILD.gn")

  if not dir_path.is_dir():
    raise ValueError(f"Error: Directory {dir_path} not found.")
  if not gn_path.is_file():
    raise ValueError(f"Error: BUILD file {gn_path} not found.")

  with open(gn_path, 'r') as f:
    content = f.read()

  group_pattern = r"group\(['\"](.+?)['\"]\) \{(.*?)\n\}"
  matched_groups = re.findall(group_pattern, content, re.DOTALL)

  if not matched_groups:
    raise ValueError(
        f"Error: Could not find group '{target_group}' in {gn_path}")
  relevant_groups = [g for g in matched_groups if g[0] == target_group]

  assert len(relevant_groups) == 1

  group_content = relevant_groups[0][1]

  data_pattern = r"data = \[(.*?)\]"
  match_data = re.search(data_pattern, group_content, re.DOTALL)

  if not match_data:
    raise ValueError(
        f"Error: Could not find data in group '{target_group}' in {gn_path}")

  data_list = match_data.group(1)

  listed_files = set(re.findall(r'["\'](.*?)["\']', data_list))
  listed_files = {Path(f.replace('//', '')) for f in listed_files}

  actual_files = {
      Path(f).relative_to(_CHROMIUM_SRC_DIR)
      for f in find_all_tests()
  }

  missing_in_gn = actual_files - listed_files

  return {str(f) for f in missing_in_gn}
