#!/usr/bin/env python
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import glob
import os
import re
import subprocess
import sys


class ClangPluginTest(object):
  """Test harness for clang plugins."""

  def __init__(self,
               test_base,
               clang_path,
               plugin_names,
               reset_results,
               filename_regex=None):
    """Constructor.

    Args:
      test_base: Path to the directory containing the tests.
      clang_path: Path to the clang binary.
      plugin_names: Names of the plugins.
      reset_results: If true, resets expected results to the actual test output.
      filename_regex: If present, only runs tests that match the regex pattern.
    """
    self._test_base = test_base
    self._clang_path = clang_path
    self._plugin_names = plugin_names
    self._reset_results = reset_results
    self._filename_regex = filename_regex

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

    clang_cmd = [self._clang_path, '-std=c++20']

    # Use the traditional diagnostics format (see crbug.com/1450229).
    clang_cmd.extend([
        '-fno-diagnostics-show-line-numbers', '-fcaret-diagnostics-max-lines=1'
    ])

    for p in self._plugin_names:
      clang_cmd.extend(['-Xclang', '-add-plugin', '-Xclang', p])
    self.AdjustClangArguments(clang_cmd)

    if not any('-fsyntax-only' in arg for arg in clang_cmd):
      clang_cmd.append('-c')

    passing = []
    failing = []
    tests = glob.glob('*.cpp') + glob.glob('*.mm')
    for test in tests:
      if self._filename_regex and not re.search(self._filename_regex, test):
        continue

      sys.stdout.write('Testing %s... ' % test)
      test_name, _ = os.path.splitext(test)

      cmd = clang_cmd[:]
      try:
        # Some tests need to run with extra flags.
        cmd.extend(open('%s.flags' % test_name).read().split())
      except IOError:
        pass
      cmd.append(test)

      print("cmd", cmd)
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
      actual = subprocess.check_output(cmd,
                                       stderr=subprocess.STDOUT,
                                       universal_newlines=True)
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

    # Normalize backslashes to forward-slashes to avoid failure on Windows
    actual = actual.replace('\\', '/')
    expected = expected.replace('\\', '/')

    if expected != actual:
      open(result_file, 'w').write(actual)
      error = 'expected and actual differed\n'
      error += 'Actual:\n' + actual
      error += 'Expected:\n' + expected
      return error
