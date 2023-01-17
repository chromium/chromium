# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''GRIT tool that runs the unit test suite for GRIT.'''


import getopt
import sys
import unittest

try:
  import grit.test_suite_all
except ImportError:
  pass
from grit.tool import interface


class UnitTestTool(interface.Tool):
  '''By using this tool (e.g. 'grit unit') you run all the unit tests for GRIT.
This happens in the environment that is set up by the basic GRIT runner.'''

  def ShortDescription(self):
    return 'Use this tool to run all the unit tests for GRIT.'

  def ParseOptions(self, args):
    """Set this objects and return all non-option arguments."""
    own_opts, args = getopt.getopt(args, '', ('help',))
    for key, val in own_opts:
      if key == '--help':
        self.ShowUsage()
        sys.exit(0)
    return args

  def Run(self, opts, args):
    args = self.ParseOptions(args)
    if args:
      print('This tool takes no arguments.')
      return 2

    return unittest.TextTestRunner(verbosity=2).run(
      grit.test_suite_all.TestSuiteAll())
