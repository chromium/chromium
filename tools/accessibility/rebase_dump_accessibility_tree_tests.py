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

from __future__ import print_function

import json
import os
import re
import sys
import tempfile
import time
from typing import List, Tuple
import urllib
import urllib.parse

# The location of the DumpAccessibilityTree html test files and expectations.
TEST_DATA_PATH = os.path.join(os.getcwd(), 'content/test/data/accessibility')

# Colors for easier debugging.
# TODO check environment  to determine whether terminal is rich or interactive.
BRIGHT_COLOR = '\033[93m'
NORMAL_COLOR = '\033[0m'

# A global that keeps track of files we've already updated, so we don't
# bother to update the same file twice.
completed_files = set()


def Fix(line):
  if line[:3] == '@@@':
    result = re.search('[^@]@([^@]*)@@@', line)
    if result:
      line = result.group(1)
  # For Android tests:
  if line[:2] == 'I ' or line[:2] == 'E ':
    result = re.search('[I|E].*run_tests_on_device\([^\)]+\)\s+(.*)', line)
    if result:
      line = result.group(1)
  # For Android content_shell_test_apk tests:
  elif line[:2] == 'C ':
    result = re.search('C\s+\d+\.\d+s Main\s+([T|E|A|a|W|\+](.*))', line)

    if result:
      line = result.group(1)
  return line


def SplitTestLogs(lines: List[str]) -> List[List[str]]:
  '''Separate log lines of many tests into groups of individual test logs.'''
  files = []
  for i, line in enumerate(lines):
    if 'Testing: ' in line:
      start_i = i
    elif '<-- End-of-file -->' in line:
      end_i = i
      files.append(lines[start_i:end_i + 1])
  return files


def WriteFileOnce(filename: str, data: List[str], directory=TEST_DATA_PATH):
  '''Write data to a file, limited to once per unique filepath.'''
  full_path = os.path.join(directory, filename)
  if full_path in completed_files:
    return
  with open(full_path, 'w') as f:
    f.writelines(data)
    completed_files.add(full_path)


def ParseLog(lines: List[str]) -> Tuple[str, str]:
  '''Parses a single failing test into an expectation file and test results.'''
  test_file = None
  expected_file = ''
  start = None
  actual_text = ''
  for i in range(len(lines)):
    line = Fix(lines[i])
    if line.find('Testing:') >= 0:
      result = re.search('content.test.*accessibility.([^@]*)', line)
      if result:
        test_file = result.group(1)
      expected_file = ''
      start = None
    if line.find('Expected output:') >= 0:
      result = re.search('content.test.*accessibility.([^@]*)', line)
      if result:
        expected_file = result.group(1)
    if line == 'Actual':
      start = i + 2
    if (start and test_file and expected_file
        and line.find('End-of-file') >= 0):
      actual = [Fix(line) for line in lines[start:i] if line]

      actual_text = '\n'.join(actual)
      # Make sure the text ends with a newline.
      if len(actual) > 0 and actual[-1][-1] != '\n':
        actual_text += '\n'

      start = None
      test_file = None
  return expected_file, actual_text


def Run():
  '''Main. Get the issue number and parse the code review page.'''
  if len(sys.argv) == 2:
    patchSetArg = '--patchset=%s' % sys.argv[1]
  else:
    patchSetArg = ''

  try:
    (fd, tmppath) = tempfile.mkstemp()
    print('Temp file: %s' % tmppath)
    os.system('git cl try-results --json %s %s' % (tmppath, patchSetArg))

    with open(tmppath) as file:
      try_result = file.read()
      if len(try_result) < 1000:
        print('Did not seem to get try bot data.')
        print(try_result)
        return
  finally:
    os.close(fd)
    os.unlink(tmppath)

  data = json.loads(try_result)

  for builder in data:
    print(builder['builder']['builder'], builder['status'])
    if builder['status'] == 'FAILURE':
      bb_command = [
          'bb',
          'get',
          builder['id'],
          '-steps',
          '-json',
      ]
      bb_command_expanded = ' '.join(bb_command)
      # print((BRIGHT_COLOR + '=> %s' + NORMAL_COLOR) % bb_command_expanded)
      output = os.popen(bb_command_expanded).read()
      steps_json = json.loads(output)

      s_name = None
      for step in steps_json['steps']:
        name = step['name']
        if (name.startswith('content_shell_test_apk') or
            name.startswith('content_browsertests')) and '(with patch)' in name:
          s_name = name

          bb_command = [
              'bb',
              'log',
              builder['id'],
              '\"%s\"' % s_name,
          ]
          bb_command_expanded = ' '.join(bb_command)
          # print((BRIGHT_COLOR + '=> %s' + NORMAL_COLOR) % bb_command_expanded)
          output = os.popen(bb_command_expanded).readlines()
          for log in SplitTestLogs(output):
            filename, actual_text = ParseLog(log)
            if actual_text:
              WriteFileOnce(filename, actual_text)
      if not output:
        print('No content_browsertests (with patch) step found')
        continue


if __name__ == '__main__':
  sys.exit(Run())
