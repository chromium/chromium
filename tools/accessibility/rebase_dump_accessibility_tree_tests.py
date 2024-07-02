#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Rebase DumpAccessibilityTree Tests.

This script is intended to be run when you make a change that could affect the
expected results of tests in:

    content/test/data/accessibility

It assumes that you've already uploaded a change and the try jobs have finished.
It collects all of the results from try jobs on all platforms and updates the
expectation files locally. Sometimes, this script will pull data from flaky test
runs, especially from content/test/data/accessibility/event/*. Run 'git diff' to
check for potentially incorrect data pulled from those tests and make sure all
of the changes look reasonable, then upload the change for code review.

Optional argument: patchset number, otherwise will default to latest patchset
'''


import json
import os
import re
import sys
from typing import List, Tuple, Optional, Generator
import subprocess

# The location of the DumpAccessibilityTree html test files and expectations.
TEST_DATA_PATH = os.path.join(os.getcwd(), 'content/test/data/accessibility')

# Colors for easier debugging.
# TODO check environment  to determine whether terminal is rich or interactive.
BRIGHT_COLOR = '\033[93m'
NORMAL_COLOR = '\033[0m'

TEST_START_STR = 'Testing: '
TEST_EXPECTED_STR = 'Expected output: '
TEST_ACTUAL_STR = 'Actual'
TEST_END_STR = '<-- End-of-file -->'

TEST_NAME_REGEX = re.compile('content.test.*accessibility.([^@]*)')

# A global that keeps track of files we've already updated, so we don't
# bother to update the same file twice.
completed_files = set()


def _clean_line(line: str) -> str:
  '''Format a line to remove unnecessary test output.'''
  if line[:3] == '@@@':
    if result := re.search('[^@]@([^@]*)@@@', line):
      line = result.group(1)
  # For Android tests:
  if line[:2] == 'I ' or line[:2] == 'E ':
    if result := re.search('[I|E].*run_tests_on_device\([^\)]+\)\s+(.*)', line):
      line = result.group(1)
  # For Android content_shell_test_apk tests:
  elif line[:2] == 'C ':
    if result := re.search('C\s+\d+\.\d+s Main\s+([T|E|A|a|W|\+](.*))', line):
      line = result.group(1)
  return line


def _get_individual_test_logs(
    log: List[str]) -> Generator[List[str], None, None]:
  '''Yields logs for an individual test from a log containing several tests.

  Each distinct expectations filename is only included once.
  '''
  expected_file = None
  for i, line in enumerate(log):
    if TEST_START_STR in line:
      start_i = i
    elif TEST_EXPECTED_STR in line:
      if result := TEST_NAME_REGEX.search(line):
        expected_file = result.group(1)
    elif TEST_END_STR in line:
      assert expected_file, "Malformed log."
      if expected_file not in completed_files:
        yield (log[start_i:i + 1])
        completed_files.add(expected_file)


def _write_file(filename: str, data: List[str], directory=TEST_DATA_PATH):
  '''Write data to a file.'''
  with open(full_path := os.path.join(directory, filename), 'w') as f:
    f.writelines(data)
    completed_files.add(full_path)
    print(f'Wrote to expectations file: {full_path}')


def _parse_log(lines: List[str]) -> Tuple[str, str]:
  '''Parses a single failing test into an expectation file and test results.'''
  test_file, expected_file, start, actual_text = None, None, None, None
  for i in range(len(lines)):
    line = lines[i]
    if TEST_START_STR in line:
      if result := TEST_NAME_REGEX.search(line):
        test_file = result.group(1)
    elif TEST_EXPECTED_STR in line:
      if result := TEST_NAME_REGEX.search(line):
        expected_file = result.group(1)
    elif TEST_ACTUAL_STR in line:
      # Skip this line (header) and the next line (separator hyphens).
      start = i + 2
    elif TEST_END_STR in line:
      actual_text = '\n'.join([_clean_line(line) for line in lines[start:i]])
      # Ensure expectation files end with a newline for consistency, even though
      #  it don't appear in test output.
      if not actual_text.endswith('\n'):
        actual_text += '\n'

  assert test_file and expected_file and actual_text, 'Malformed log.'
  return (expected_file, actual_text)


def get_trybot_log(patch_set: Optional[int]) -> List:
  '''Get trybot data for the current branch's issue.'''
  patch_set_arg = f'--patchset={patch_set}' if patch_set is not None else ''

  return json.loads(
      subprocess.run(f'git cl try-results --json=- {patch_set_arg}',
                     shell=True,
                     capture_output=True,
                     text=True).stdout)


def _get_builder_steps(
    builder_ids: Generator[str, None, None]
) -> Generator[Tuple[str, str], None, None]:
  '''Yields (builder_id, content_test_step_name) for the provided builders.'''

  def _is_content_test(name: str) -> bool:
    return (name.startswith('content_shell_test_apk') or
            name.startswith('content_browsertests')) and '(with patch)' in name

  for b in builder_ids:
    steps = json.loads(
        subprocess.run(f'bb get {b} -steps -json',
                       shell=True,
                       capture_output=True,
                       text=True).stdout)['steps']
    step_names = (s["name"] for s in steps if _is_content_test(s["name"]))
    for s in step_names:
      yield (b, s)


def main():
  patch_set = sys.argv[1] if len(sys.argv) > 1 else None

  failing_builder_ids = (b["id"] for b in get_trybot_log(patch_set)
                         if b['status'] == 'FAILURE')

  for (b, s) in _get_builder_steps(failing_builder_ids):
    output = subprocess.run(f'bb log {b} "{s}"',
                            shell=True,
                            capture_output=True,
                            text=True).stdout.split('\n')
    for log in _get_individual_test_logs(output):
      _write_file(*_parse_log(log))

if __name__ == '__main__':
  sys.exit(main())
