#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
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
import urlparse

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
    try:
      line = re.search('[^@]@([^@]*)@@@', line).group(1)
    except:
      pass
  # For Android tests:
  if line[:2] == 'I ':
    try:
      line = re.search('I  \d+\.\d+s run_tests_on_device\([0-9a-f]+\)  (.*)',
                       line).group(1)
    except:
      pass
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
      test_file = re.search(
          'content.test.*accessibility.([^@]*)', line).group(1)
      expected_file = None
      start = None
    if line.find('Expected output:') >= 0:
      expected_file = re.search(
          'content.test.*accessibility.([^@]*)', line).group(1)
    if line == 'Actual':
      start = i + 2
    if start and test_file and expected_file and line.find('End-of-file') >= 0:
      dst_fullpath = os.path.join(TEST_DATA_PATH, expected_file)
      if dst_fullpath in completed_files:
        continue

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

  (_, tmppath) = tempfile.mkstemp()
  print('Temp file: %s' % tmppath)
  os.system('git cl try-results --json %s %s' % (tmppath, patchSetArg))

  try_result = open(tmppath).read()
  if len(try_result) < 1000:
    print('Did not seem to get try bot data.')
    print(try_result)
    return

  data = json.loads(try_result)
  os.unlink(tmppath)

  #print(json.dumps(data, indent=4))

  for builder in data:
    print(builder['builder']['builder'], builder['status'])
    if builder['status'] == 'FAILURE':
      logdog_tokens = [
          'chromium',
          'buildbucket',
          'cr-buildbucket.appspot.com',
          builder['id'],
          '+',
          'steps',
          '**']
      logdog_path = '/'.join(logdog_tokens)
      logdog_query = 'cit logdog query -results 999 -path "%s"' % logdog_path
      print((BRIGHT_COLOR + '=> %s' + NORMAL_COLOR) % logdog_query)
      steps = os.popen(logdog_query).readlines()
      a11y_step = None
      for step in steps:
        if (step.find('/content_browsertests') >= 0 and
            step.find('with_patch') >= 0 and
            step.find('trigger') == -1 and
            step.find('swarming.summary') == -1 and
            step.find('step_metadata') == -1 and
            step.find('Upload') == -1):

          a11y_step = step.rstrip()
          logdog_cat = 'cit logdog cat -raw "%s"' % a11y_step
          # A bit noisy but useful for debugging.
          # print((BRIGHT_COLOR + '=> %s' + NORMAL_COLOR) % logdog_cat)
          output = os.popen(logdog_cat).read()
          ParseLog(output)
      if not a11y_step:
        print('No content_browsertests (with patch) step found')
        continue

if __name__ == '__main__':
  sys.exit(Run())
