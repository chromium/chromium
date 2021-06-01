#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for the 'grit buildinfo' tool.
"""

from __future__ import print_function

import os
import sys
import unittest

# This is needed to find some of the imports below.
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

from six import StringIO

# pylint: disable-msg=C6204
from grit.tool import buildinfo


class BuildInfoUnittest(unittest.TestCase):
  def setUp(self):
    self.old_cwd = os.getcwd()
    # Change CWD to make tests work independently of callers CWD.
    os.chdir(os.path.dirname(__file__))
    os.chdir('..')
    self.buf = StringIO()
    self.old_stdout = sys.stdout
    sys.stdout = self.buf

  def tearDown(self):
    sys.stdout = self.old_stdout
    os.chdir(self.old_cwd)

  def testBuildOutput(self):
    """Find all of the inputs and outputs for a GRD file."""
    info_object = buildinfo.DetermineBuildInfo()

    class DummyOpts(object):
      def __init__(self):
        self.input = '../grit/testdata/buildinfo.grd'
        self.print_header = False
        self.verbose = False
        self.extra_verbose = False
    info_object.Run(DummyOpts(), [])
    output = self.buf.getvalue().replace('\\', '/')
    self.failUnless(output.count(r'rc_all|sv_sidebar_loading.html'))
    self.failUnless(output.count(r'rc_header|resource.h'))
    self.failUnless(output.count(r'rc_all|en_generated_resources.rc'))
    self.failUnless(output.count(r'rc_all|sv_generated_resources.rc'))
    self.failUnless(output.count(r'input|../grit/testdata/substitute.xmb'))
    self.failUnless(output.count(r'input|../grit/testdata/pr.bmp'))
    self.failUnless(output.count(r'input|../grit/testdata/pr2.bmp'))
    self.failUnless(
        output.count(r'input|../grit/testdata/sidebar_loading.html'))
    self.failUnless(output.count(r'input|../grit/testdata/transl.rc'))
    self.failUnless(output.count(r'input|../grit/testdata/transl1.rc'))

  def testBuildOutputWithDir(self):
    """Find all the inputs and outputs for a GRD file with an output dir."""
    info_object = buildinfo.DetermineBuildInfo()

    class DummyOpts(object):
      def __init__(self):
        self.input = '../grit/testdata/buildinfo.grd'
        self.print_header = False
        self.verbose = False
        self.extra_verbose = False
    info_object.Run(DummyOpts(), ['-o', '../grit/testdata'])
    output = self.buf.getvalue().replace('\\', '/')
    self.failUnless(
        output.count(r'rc_all|../grit/testdata/sv_sidebar_loading.html'))
    self.failUnless(output.count(r'rc_header|../grit/testdata/resource.h'))
    self.failUnless(
        output.count(r'rc_all|../grit/testdata/en_generated_resources.rc'))
    self.failUnless(
        output.count(r'rc_all|../grit/testdata/sv_generated_resources.rc'))
    self.failUnless(output.count(r'input|../grit/testdata/substitute.xmb'))
    self.failUnlessEqual(0,
        output.count(r'rc_all|../grit/testdata/sv_welcome_toast.html'))
    self.failUnless(
        output.count(r'rc_all|../grit/testdata/en_welcome_toast.html'))


if __name__ == '__main__':
  unittest.main()
