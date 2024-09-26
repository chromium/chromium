#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This is a script helping developer bisect test failures.
#
# Currently this only supports bisecting gtest based test failures.
# Say you're assigned a BrowserTest.TestCase1 failure. You would generally do
# 1 Find a good commit and bad commit.
# 2 `git bisect start`
# 3 `git bisect good <good commit id>`
# 4 `git bisect bad <bad commit id>`
# 5 `gclient sync`
# 6 `autoninja -C out/Default browser_tests`
# 7 `out/Default/browser_tests --gtest_filter=BrowserTest.TestCase1`
# 8 if the test pass, `git bisect good`, otherwise `git bisect bad`.
# 9 repeat 5 - 8 until finding the culprit.
# This script will help you on 2 - 9. You first do 1, then run
# `python3 tools/bisect/bisect_gtests.py -g <good commit id> -b <bad commit id>
#   --build_command 'autoninja -C out/Default browser_tests'
#   --test_command 'out/Default/browser_tests
#                   --gtest_filter=BrowserTest.TestCase1'`
# The script will run until it finds the culprit cl breaking the test.
#
# Note1: We only support non-flaky -> failure, or non-flaky -> flaky.
# Flaky -> failure can't get correct result. For non-flaky -> flaky,
# you can use `--gtest_repeat`.
# Note2: For tests using python launching script, this is supported. e.g.
# `--test_command 'build/lacros/test_runner.py test
#      out/lacrosdesktop/lacros_chrome_browsertests
#      --ash-chrome-path=out/lacrosdesktop/ash_clang_x64/test_ash_chrome
#      --gtest_filter=BrowserTest.TestCase1'`

import argparse
import subprocess
import sys

# This is the message from `git bisect` when it
# finds the culprit cl.
GIT_BAD_COMMENT_MSG = 'is the first bad commit'
GIT_BISECT_IN_PROCESS_MSG = 'left to test after this'


def Run(command, print_stdout_on_error=True):
  print(command)
  c = subprocess.run(command, shell=True)
  if print_stdout_on_error and c.returncode != 0:
    print(c.stdout)
  return c.returncode == 0


def StartBisect(good_rev, bad_rev, build_command, test_command):
  assert (Run('git bisect start'))
  assert (Run('git bisect bad %s' % bad_rev))
  assert (Run('git bisect good %s' % good_rev))

  while True:
    assert (Run('gclient sync'))
    assert (Run(build_command))
    test_ret = None
    # If the test result is different running twice, then
    # try again.
    for _ in range(5):
      c1 = Run(test_command, print_stdout_on_error=False)
      c2 = Run(test_command, print_stdout_on_error=False)
      if c1 == c2:
        test_ret = c2
        break

    gitcp = None
    if test_ret:
      print('git bisect good')
      gitcp = subprocess.run('git bisect good',
                             shell=True,
                             capture_output=True,
                             text=True)
    else:
      print('git bisect bad')
      gitcp = subprocess.run('git bisect bad',
                             shell=True,
                             capture_output=True,
                             text=True)
    # git should always print 'left to test after this'. No stdout
    # means something is wrong.
    if not gitcp.stdout:
      print('Something is wrong! Exit bisect.')
      if gitcp.stderr:
        print(gitcp.stderr)
      break

    print(gitcp.stdout)
    first_line = gitcp.stdout[:gitcp.stdout.find('\n')]
    # Found the culprit!
    if GIT_BAD_COMMENT_MSG in first_line:
      print('Found the culprit change!')
      return 0
    if GIT_BISECT_IN_PROCESS_MSG not in first_line:
      print('Something is wrong! Exit bisect.')
      if gitcp.stderr:
        print(gitcp.stderr)
      break
  return 1


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('-b',
                      '--bad',
                      type=str,
                      help='A bad revision to start bisection.')
  parser.add_argument('-g',
                      '--good',
                      type=str,
                      help='A good revision to start bisection.')
  parser.add_argument('--build_command',
                      type=str,
                      help='Command to build test target.')
  parser.add_argument('--test_command', type=str, help='Command to run test.')
  args = parser.parse_args()
  return StartBisect(args.good, args.bad, args.build_command, args.test_command)


if __name__ == '__main__':
  sys.exit(main())
