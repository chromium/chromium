# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
from pathlib import Path
from typing import Iterable, List, Set, Dict
from dataclasses import dataclass
import tempfile

import setup_modules
import chromium_src.tools.metrics.python_support.dependency_solver as dependency_solver

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
_CHROMIUM_SRC_DIR = os.path.dirname(os.path.dirname(os.path.dirname(_THIS_DIR)))
_TOOLS_METRICS_RELATIVE_PATH = os.path.join('tools', 'metrics')
_TOOLS_METRICS_DIR = os.path.join(_CHROMIUM_SRC_DIR,
                                  _TOOLS_METRICS_RELATIVE_PATH)

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


def _get_temp_path(prefix=''):
  fd, path = tempfile.mkstemp(prefix=prefix)
  os.close(fd)
  return path


@dataclass
class TestableScript:
  """Describe a script that can be safely run for testing"""
  # The name to identify the script by
  identifiable_name: str

  # Path relative to src/
  file_path: Path

  # Full cmd to run with args included
  cmd: List[str]

  @classmethod
  def CreatePythonScript(cls,
                         file_path: Path,
                         flags: List[str] = []) -> 'TestableScript':
    """ Creates TestableScript object for python script

    Args:
      file_path - a path to python script relative to src/
      flags - a list of flags to pass to the script

    Returns:
      TestableScript object set up to run using vpython3
    """
    return TestableScript(identifiable_name=str(file_path),
                          file_path=file_path,
                          cmd=['vpython3', str(file_path), *flags])


# As one of the check we just run some of our existing scripts that don't have
# side effects and check if they finish successfully this is done as a last
# line of defense against changing in dependencies causing failures of scripts.
_TESTABLE_SCRIPTS: List[TestableScript] = [
    TestableScript.CreatePythonScript(
        file_path=Path('tools/metrics/actions/extract_actions.py')),
    TestableScript.CreatePythonScript(
        file_path=Path('tools/metrics/actions/pretty_print.py')),
    TestableScript.CreatePythonScript(
        file_path=Path('tools/metrics/actions/print_action_names.py')),
    TestableScript.CreatePythonScript(
        file_path=Path('tools/metrics/histograms/merge_xml.py'),
        flags=['--output', _get_temp_path('merge_xml_test')]),
    TestableScript.CreatePythonScript(
        file_path=Path('tools/metrics/histograms/pretty_print.py'),
        flags=['tools/metrics/histograms/metadata/uma/histograms.xml']),
    TestableScript.CreatePythonScript(file_path=Path(
        'tools/metrics/histograms/print_expanded_histograms.py')),
    TestableScript.CreatePythonScript(
        file_path=Path('tools/metrics/histograms/print_histogram_names.py')),
    TestableScript.CreatePythonScript(
        file_path=Path('tools/metrics/histograms/validate_format.py')),
    TestableScript.CreatePythonScript(file_path=Path(
        'tools/metrics/histograms/validate_histograms_index.py')),
    TestableScript.CreatePythonScript(
        file_path=Path('tools/metrics/histograms/validate_token.py'),
        flags=['tools/metrics/histograms/metadata/uma/histograms.xml']),
    TestableScript.CreatePythonScript(
        file_path=Path('tools/metrics/private_metrics/pretty_print.py'),
        flags=['tools/metrics/private_metrics/dwa.xml']),
    TestableScript.CreatePythonScript(
        file_path=Path('tools/metrics/private_metrics/validate_format.py'),
        flags=['tools/metrics/private_metrics/dwa.xml']),
    TestableScript.CreatePythonScript(
        file_path=Path('tools/metrics/ukm/pretty_print.py'), flags=[]),
    TestableScript.CreatePythonScript(
        file_path=Path('tools/metrics/ukm/validate_format.py')),
    # TODO(crbug.com/488367077): Fix this script.
    # TestableScript.CreatePythonScript(
    #   file_path='tools/metrics/histograms/histogram_ownership.py'
    # ),
    # TODO(crbug.com/488362727): Fix the unmapped histograms.
    # TestableScript.CreatePythonScript(file_path=Path(
    #      'tools/metrics/histograms/find_unmapped_histograms.py')),
]


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

  if len(relevant_groups) != 1:
    raise ValueError(
        f"Error: Could not find group '{target_group}' in {gn_path}")

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


def _is_script_affected_by(testable_script: TestableScript,
                           modified_files: Set[Path],
                           deps_graph: Dict[str, List[str]]) -> bool:
  modified_files = set(
      [p.relative_to(_CHROMIUM_SRC_DIR) for p in modified_files])
  if testable_script.file_path in modified_files:
    return True
  all_dependencies = dependency_solver.get_all_dependencies(
      deps_graph, str(testable_script.file_path))
  return any(file in all_dependencies for file in modified_files)


def get_affected_testable_scripts(
    modified_files: Set[Path],
    deps_graph: Dict[str, List[str]]) -> List[TestableScript]:
  """Returns testable scripts that are affected by changes in modified_files.

  Args:
    modified_files: list of modified files as absolute paths that can
      affects _TESTABLE_SCRIPTS.
  """
  return [
      testable_script
      for testable_script in _TESTABLE_SCRIPTS if _is_script_affected_by(
          testable_script, set(modified_files), deps_graph)
  ]


def get_affected_tests(
    modified_files: Set[Path],
    deps_graph: Dict[str, List[str]]) -> Iterable[TestableScript]:
  """Finds all python tests that could be affected by the changes."""
  all_tests = [
      TestableScript.CreatePythonScript(Path(t).relative_to(_CHROMIUM_SRC_DIR))
      for t in find_all_tests()
      # We cannot detect dependencies for tools outside of tools/metrics
      # at the moment.
      if Path(t).resolve().is_relative_to(_TOOLS_METRICS_DIR)
  ]

  affected_tests = [
      test_script for test_script in all_tests
      if _is_script_affected_by(test_script, set(modified_files), deps_graph)
  ]
  return affected_tests
