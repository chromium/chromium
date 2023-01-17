#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for the 'grit buildinfo' tool.
"""

import io
import os
import sys
import unittest

# This is needed to find some of the imports below.
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

# pylint: disable-msg=C6204
from grit.tool import buildinfo


class BuildInfoUnittest(unittest.TestCase):
  def setUp(self):
    self.old_cwd = os.getcwd()
    # Change CWD to make tests work independently of callers CWD.
    os.chdir(os.path.dirname(__file__))
    os.chdir('..')
    self.buf = io.StringIO()
    self.old_stdout = sys.stdout
    sys.stdout = self.buf

  def tearDown(self):
    sys.stdout = self.old_stdout
    os.chdir(self.old_cwd)

  def testBuildOutput(self):
    """Find all of the inputs and outputs for a GRD file."""
    info_object = buildinfo.DetermineBuildInfo()

    class DummyOpts:
      def __init__(self):
        self.input = '../grit/testdata/buildinfo.grd'
        self.print_header = False
        self.verbose = False
        self.extra_verbose = False
    info_object.Run(DummyOpts(), [])
    output = self.buf.getvalue().replace('\\', '/')
    self.assertTrue(output.count(r'rc_all|sv_sidebar_loading.html'))
    self.assertTrue(output.count(r'rc_header|resource.h'))
    self.assertTrue(output.count(r'rc_all|en_generated_resources.rc'))
    self.assertTrue(output.count(r'rc_all|sv_generated_resources.rc'))
    self.assertTrue(output.count(r'input|../grit/testdata/substitute.xmb'))
    self.assertTrue(output.count(r'input|../grit/testdata/pr.bmp'))
    self.assertTrue(output.count(r'input|../grit/testdata/pr2.bmp'))
    self.assertTrue(
        output.count(r'input|../grit/testdata/sidebar_loading.html'))
    self.assertTrue(output.count(r'input|../grit/testdata/transl.rc'))
    self.assertTrue(output.count(r'input|../grit/testdata/transl1.rc'))

  def testBuildOutputWithDir(self):
    """Find all the inputs and outputs for a GRD file with an output dir."""
    info_object = buildinfo.DetermineBuildInfo()

    class DummyOpts:
      def __init__(self):
        self.input = '../grit/testdata/buildinfo.grd'
        self.print_header = False
        self.verbose = False
        self.extra_verbose = False
    info_object.Run(DummyOpts(), ['-o', '../grit/testdata'])
    output = self.buf.getvalue().replace('\\', '/')
    self.assertTrue(
        output.count(r'rc_all|../grit/testdata/sv_sidebar_loading.html'))
    self.assertTrue(output.count(r'rc_header|../grit/testdata/resource.h'))
    self.assertTrue(
        output.count(r'rc_all|../grit/testdata/en_generated_resources.rc'))
    self.assertTrue(
        output.count(r'rc_all|../grit/testdata/sv_generated_resources.rc'))
    self.assertTrue(output.count(r'input|../grit/testdata/substitute.xmb'))
    self.assertEqual(0,
        output.count(r'rc_all|../grit/testdata/sv_welcome_toast.html'))
    self.assertTrue(
        output.count(r'rc_all|../grit/testdata/en_welcome_toast.html'))


if __name__ == '__main__':
  unittest.main()
