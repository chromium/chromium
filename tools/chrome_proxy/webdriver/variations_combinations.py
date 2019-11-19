# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import io
import os
import platform
import sys
import time
import unittest

import common

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir, os.pardir,
  os.pardir, 'tools', 'variations'))
import fieldtrial_util

test_blacklist = [
  # These tests set their own field trials and should be ignored.
  'quic.Quic.testCheckPageWithQuicProxy',
  'quic.Quic.testCheckPageWithQuicProxyTransaction',
  'smoke.Smoke.testCheckPageWithHoldback',
]

def GetExperimentArgs():
  """Returns a list of arguments with all tested field trials.

  This function is a simple wrapper around the variation team's fieldtrail_util
  script that generates command line arguments to test Chromium field trials.

  Returns:
    an array of command line arguments to pass to chrome
  """
  config_path = os.path.join(os.path.dirname(__file__), os.pardir, os.pardir,
    os.pardir, 'testing', 'variations', 'fieldtrial_testing_config.json')
  my_platform = ''
  if common.ParseFlags().android:
    my_platform = 'android'
  elif platform.system().lower() == 'linux':
    my_platform = 'linux'
  elif platform.system().lower() == 'windows':
    my_platform = 'windows'
  elif platform.system().lower() == 'darwin':
    my_platform = 'mac'
  else:
    raise Exception('unknown platform!')
  return fieldtrial_util.GenerateArgs(config_path, [my_platform])

def GenerateTestSuites():
  """A generator function that yields non-blacklisted tests to run.

  This function yields test suites each with a single test case whose id is not
  blacklisted in the array at the top of this file.

  Yields:
    non-blacklisted test suites to run
  """
  loader = unittest.TestLoader()
  for test_suite in loader.discover(os.path.dirname(__file__), pattern='*.py'):
    for test_case in test_suite:
      for test_method in test_case:
        if test_method.id() not in test_blacklist:
          ts = unittest.TestSuite()
          ts.addTest(test_method)
          yield (ts, test_method.id())

def ParseFlagsWithExtraBrowserArgs(extra_args):
  """Generates a function to override common.ParseFlags.

  The returned function will honor everything in the original ParseFlags(), but
  adds on additional browser_args.

  Args:
    extra_args: The extra browser agruments to add.
  Returns:
    A function to override common.ParseFlags with additional browser_args.
  """
  original_flags = common.ParseFlags()
  def AddExtraBrowserArgs():
    original_flags.browser_args = ((original_flags.browser_args if
      original_flags.browser_args else '') + ' ' + extra_args)
    return original_flags
  return AddExtraBrowserArgs

def main():
  """Runs all non-blacklisted tests against Chromium field trials.

  This script run all chrome proxy integration tests that haven't been
  blacklisted against the field trial testing configuration used by Chromium
  perf bots.
  """
  flags = common.ParseFlags()
  experiment_args = ' '.join(GetExperimentArgs())
  common.ParseFlags = ParseFlagsWithExtraBrowserArgs(experiment_args)
  # Each test is wrapped in its own test suite so results can be evaluated
  # individually.
  for test_suite, test_id in GenerateTestSuites():
    buf = io.BytesIO()
    sys.stdout.write('%s... ' % test_id)
    sys.stdout.flush()
    testRunner = unittest.runner.TextTestRunner(stream=buf, verbosity=2,
      buffer=(not flags.disable_buffer))
    result = testRunner.run(test_suite)
    if result.wasSuccessful():
      print('ok')
    else:
      print('failed')
      print(buf.getvalue())
      print('To repeat this test, run: ')
      print("%s %s %s --test_filter=%s --browser_args='%s'" % (
          sys.executable,
          os.path.join(os.path.dirname(__file__), 'run_all_tests.py'), ' '.join(
              sys.argv[1:]), '.'.join(test_id.split('.')[1:]), experiment_args))
      if flags.failfast:
        return

if __name__ == '__main__':
  main()
