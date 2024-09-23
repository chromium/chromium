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
from typing import List, Tuple, Optional, Generator, Iterable
import subprocess
import requests

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
      assert expected_file, 'Malformed log.'
      if expected_file not in completed_files:
        yield (log[start_i:i + 1])
        completed_files.add(expected_file)


def _write_file(filename: str, data: List[str], directory=TEST_DATA_PATH):
  '''Write data to a file.'''
  with open(full_path := os.path.join(directory, filename), 'w') as f:
    f.writelines(data)
    completed_files.add(full_path)
    print(".", end="", flush=True)


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

  def _ensure_luci_logged_in():
    '''Ensure we are logged into LUCI, as `git cl try-results` will log a
     warning otherwise.'''
    process = subprocess.run(
        f'luci-auth token -scopes https://www.googleapis.com/auth/userinfo.email',
        shell=True,
        capture_output=True,
    )
    if (process.returncode != 0):
      raise PermissionError(
          'Not logged into LUCI, please run `luci-auth login`')

  _ensure_luci_logged_in()
  patch_set_arg = f'--patchset={patch_set}' if patch_set is not None else ''
  if not (output := subprocess.run(
      f'git cl try-results --json=- {patch_set_arg}',
      shell=True,
      capture_output=True,
      text=True,
  ).stdout):
    raise ValueError('Did not find an issue attached to the current branch.')
  return json.loads(output)


def _rdb_rpc(method: str, request: dict) -> dict:
  '''Calls a given `rdb` RPC method.

  Args:
    method: The method to call. Must be within luci.resultdb.v1.ResultDB.
    request: The request, in dict format.

  Returns:
    The response from ResultDB, in dict format.
  '''
  p = subprocess.Popen(
      f'rdb rpc luci.resultdb.v1.ResultDB {method}',
      shell=True,
      stdin=subprocess.PIPE,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      text=True,
  )
  stdout, stderr = p.communicate(json.dumps(request))
  if p.returncode != 0:
    raise Exception(f'rdb rpc {method} failed with: {stderr}')
  return json.loads(stdout)


def _get_artifacts_for_failing_tests(builder_id: str) -> List[str]:
  '''Gets all the failing artifact URLs for a builder.

  Queries one builder at a time, because querying for more may timeout (see
  b/350991029 for context).
  '''
  response = _rdb_rpc(
      'QueryArtifacts',
      {
          'invocations': [f'invocations/build-{builder_id}'],
          'pageSize': 1000,
          'predicate': {
              'testResultPredicate': {
                  'expectancy': 'VARIANTS_WITH_ONLY_UNEXPECTED_RESULTS',
              }
          },
      },
  )
  if 'artifacts' not in response:
    return []

  def _has_text_log(artifact: dict) -> bool:
    '''Returns whether an artifact is a text log.'''
    return 'contentType' in artifact and artifact['contentType'] == 'text/plain'

  return [
      artifact['fetchUrl'] for artifact in response['artifacts']
      if _has_text_log(artifact)
  ]


def main():
  patch_set = sys.argv[1] if len(sys.argv) > 1 else None

  failing_builder_ids = [
      b['id'] for b in get_trybot_log(patch_set) if b['status'] == 'FAILURE'
  ]
  if not failing_builder_ids:
    print('No failing builders found for the current branch.')
    return

  # A single session to prevent throttling and timeouts.
  s = requests.Session()

  for builder_id in failing_builder_ids:
    for url in _get_artifacts_for_failing_tests(builder_id):
      test_log = s.get(url).text.split('\n')
      for log in _get_individual_test_logs(test_log):
        expected_file, actual_text = _parse_log(log)
        _write_file(expected_file, actual_text)
  sorted_files = sorted(completed_files)
  print(''.join([f'\nWrote expectations file: {f}' for f in sorted_files]))

if __name__ == '__main__':
  sys.exit(main())
