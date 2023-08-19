#!/usr/bin/env python3
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Rebase DumpAccessibilityTree Tests.

This script is intended to be run when you make a change that could affect the
expected results of tests in:

    content/test/data/accessibility

It assumes that you've already uploaded a change and the try jobs have finished.
It collects all of the results from try jobs on all platforms and updates the
expectation files locally. From there you can run 'git diff' to make sure all
of the changes look reasonable, then upload the change for code review.

Optional argument: patchset number, otherwise will default to latest patchset
"""

from __future__ import print_function

import json
import os
import re
import sys
import tempfile
import time
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
  if line[:2] == 'I ':
    result = re.search('I.*run_tests_on_device\([^\)]+\)\s+(.*)',
                       line)
    if result:
      line = result.group(1)
  # For Android content_shell_test_apk tests:
  elif line[:2] == 'C ':
    result = re.search('C\s+\d+\.\d+s Main\s+([T|E|A|a|W|\+](.*))', line)

    if result:
      line = result.group(1)
  return line

def ParseLog(logdata):
  '''Parse the log file for failing tests and overwrite the expected
     result file locally with the actual results from the log.'''
  lines = logdata.splitlines()
  test_file = None
  expected_file = None
  start = None
  for i in range(len(lines)):
    line = Fix(lines[i])
    if line.find('Testing:') >= 0:
      result = re.search('content.test.*accessibility.([^@]*)', line)
      if result:
        test_file = result.group(1)
      expected_file = None
      start = None
    if line.find('Expected output:') >= 0:
      result = re.search('content.test.*accessibility.([^@]*)', line)
      if result:
        expected_file = result.group(1)
    if line == 'Actual':
      start = i + 2
    if start and test_file and expected_file and line.find('End-of-file') >= 0:
      dst_fullpath = os.path.join(TEST_DATA_PATH, expected_file)
      if dst_fullpath in completed_files:
        continue

      if line[:3] != '---':
        start = start + 1  # Skip separator line of hyphens
      actual = [Fix(line) for line in lines[start : i] if line]
      fp = open(dst_fullpath, 'w')
      fp.write('\n'.join(actual))
      fp.close()
      print("* %s" % os.path.relpath(dst_fullpath))
      completed_files.add(dst_fullpath)
      start = None
      test_file = None
      expected_file = None

def Run():
  '''Main. Get the issue number and parse the code review page.'''
  if len(sys.argv) == 2:
    patchSetArg = '--patchset=%s' % sys.argv[1]
  else:
    patchSetArg = '';

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
      ParseLog('\n'.join(output))
      if not output:
        print('No content_browsertests (with patch) step found')
        continue

if __name__ == '__main__':
  sys.exit(Run())
