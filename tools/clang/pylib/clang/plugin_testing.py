#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import glob
import os
import subprocess
import sys


class ClangPluginTest(object):
  """Test harness for clang plugins."""

  def __init__(self, test_base, clang_path, plugin_name, reset_results):
    """Constructor.

    Args:
      test_base: Path to the directory containing the tests.
      clang_path: Path to the clang binary.
      plugin_name: Name of the plugin.
      reset_results: If true, resets expected results to the actual test output.
    """
    self._test_base = test_base
    self._clang_path = clang_path
    self._plugin_name = plugin_name
    self._reset_results = reset_results

  def AddPluginArg(self, clang_cmd, plugin_arg):
    """Helper to add an argument for the tested plugin."""
    clang_cmd.extend(['-Xclang', '-plugin-arg-%s' % self._plugin_name,
                      '-Xclang', plugin_arg])

  def AdjustClangArguments(self, clang_cmd):
    """Tests can override this to customize the command line for clang."""
    pass

  def Run(self):
    """Runs the tests.

    The working directory is temporarily changed to self._test_base while
    running the tests.

    Returns: the number of failing tests.
    """
    print('Using clang %s...' % self._clang_path)

    os.chdir(self._test_base)

    clang_cmd = [self._clang_path, '-c', '-std=c++14']
    clang_cmd.extend(['-Xclang', '-add-plugin', '-Xclang', self._plugin_name])
    self.AdjustClangArguments(clang_cmd)

    passing = []
    failing = []
    tests = glob.glob('*.cpp')
    for test in tests:
      sys.stdout.write('Testing %s... ' % test)
      test_name, _ = os.path.splitext(test)

      cmd = clang_cmd[:]
      try:
        # Some tests need to run with extra flags.
        cmd.extend(file('%s.flags' % test_name).read().split())
      except IOError:
        pass
      cmd.append(test)

      failure_message = self.RunOneTest(test_name, cmd)
      if failure_message:
        print('failed: %s' % failure_message)
        failing.append(test_name)
      else:
        print('passed!')
        passing.append(test_name)

    print('Ran %d tests: %d succeeded, %d failed' % (
        len(passing) + len(failing), len(passing), len(failing)))
    for test in failing:
      print('    %s' % test)
    return len(failing)

  def RunOneTest(self, test_name, cmd):
    try:
      actual = subprocess.check_output(cmd, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
      # Some plugin tests intentionally trigger compile errors, so just ignore
      # an exit code that indicates failure.
      actual = e.output
    except Exception as e:
      return 'could not execute %s (%s)' % (cmd, e)

    return self.ProcessOneResult(test_name, actual)

  def ProcessOneResult(self, test_name, actual):
    """Tests can override this for custom result processing."""
    # On Windows, clang emits CRLF as the end of line marker. Normalize it to LF
    # to match posix systems.
    actual = actual.replace('\r\n', '\n')

    result_file = '%s.txt%s' % (test_name, '' if self._reset_results else
                                '.actual')
    try:
      expected = open('%s.txt' % test_name).read()
    except IOError:
      open(result_file, 'w').write(actual)
      return 'no expected file found'

    if expected != actual:
      open(result_file, 'w').write(actual)
      error = 'expected and actual differed\n'
      error += 'Actual:\n' + actual
      error += 'Expected:\n' + expected
      return error
