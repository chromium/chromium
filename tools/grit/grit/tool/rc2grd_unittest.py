#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.tool.rc2grd'''

from __future__ import print_function

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

import re
import unittest

from six import StringIO

from grit import grd_reader
from grit import util
from grit.node import base
from grit.tool import rc2grd


class Rc2GrdUnittest(unittest.TestCase):
  def testPlaceholderize(self):
    tool = rc2grd.Rc2Grd()
    original = "Hello %s, how are you? I'm $1 years old!"
    msg = tool.Placeholderize(original)
    self.failUnless(msg.GetPresentableContent() == "Hello TODO_0001, how are you? I'm TODO_0002 years old!")
    self.failUnless(msg.GetRealContent() == original)

  def testHtmlPlaceholderize(self):
    tool = rc2grd.Rc2Grd()
    original = "Hello <b>[USERNAME]</b>, how are you? I'm [AGE] years old!"
    msg = tool.Placeholderize(original)
    self.failUnless(msg.GetPresentableContent() ==
                    "Hello BEGIN_BOLDX_USERNAME_XEND_BOLD, how are you? I'm X_AGE_X years old!")
    self.failUnless(msg.GetRealContent() == original)

  def testMenuWithoutWhitespaceRegression(self):
    # There was a problem in the original regular expression for parsing out
    # menu sections, that would parse the following block of text as a single
    # menu instead of two.
    two_menus = '''
// Hyper context menus
IDR_HYPERMENU_FOLDER MENU
BEGIN
    POPUP "HyperFolder"
    BEGIN
        MENUITEM "Open Containing Folder",      IDM_OPENFOLDER
    END
END

IDR_HYPERMENU_FILE MENU
BEGIN
    POPUP "HyperFile"
    BEGIN
        MENUITEM "Open Folder",                 IDM_OPENFOLDER
    END
END

'''
    self.failUnless(len(rc2grd._MENU.findall(two_menus)) == 2)

  def testRegressionScriptWithTranslateable(self):
    tool = rc2grd.Rc2Grd()

    # test rig
    class DummyNode(base.Node):
      def AddChild(self, item):
        self.node = item
      verbose = False
      extra_verbose = False
    tool.not_localizable_re = re.compile('')
    tool.o = DummyNode()

    rc_text = '''STRINGTABLE\nBEGIN\nID_BINGO "<SPAN id=hp style='BEHAVIOR: url(#default#homepage)'></SPAN><script>if (!hp.isHomePage('[$~HOMEPAGE~$]')) {document.write(""<a href=\\""[$~SETHOMEPAGEURL~$]\\"" >Set As Homepage</a> - "");}</script>"\nEND\n'''
    tool.AddMessages(rc_text, tool.o)
    self.failUnless(tool.o.node.GetCdata().find('Set As Homepage') != -1)

    # TODO(joi) Improve the HTML parser to support translateables inside
    # <script> blocks?
    self.failUnless(tool.o.node.attrs['translateable'] == 'false')

  def testRoleModel(self):
    rc_text = ('STRINGTABLE\n'
               'BEGIN\n'
               '  // This should not show up\n'
               '  IDS_BINGO "Hello %s, how are you?"\n'
               '  // The first description\n'
               '  IDS_BONGO "Hello %s, my name is %s, and yours?"\n'
               '  IDS_PROGRAMS_SHUTDOWN_TEXT      "Google Desktop Search needs to close the following programs:\\n\\n$1\\nThe installation will not proceed if you choose to cancel."\n'
               'END\n')
    tool = rc2grd.Rc2Grd()
    tool.role_model = grd_reader.Parse(StringIO(
      '''<?xml version="1.0" encoding="UTF-8"?>
      <grit latest_public_release="2" source_lang_id="en-US" current_release="3" base_dir=".">
        <release seq="3">
          <messages>
            <message name="IDS_BINGO">
              Hello <ph name="USERNAME">%s<ex>Joi</ex></ph>, how are you?
            </message>
            <message name="IDS_BONGO" desc="The other description">
              Hello <ph name="USERNAME">%s<ex>Jakob</ex></ph>, my name is <ph name="ADMINNAME">%s<ex>Joi</ex></ph>, and yours?
            </message>
            <message name="IDS_PROGRAMS_SHUTDOWN_TEXT" desc="LIST_OF_PROGRAMS is replaced by a bulleted list of program names.">
              Google Desktop Search needs to close the following programs:

<ph name="LIST_OF_PROGRAMS">$1<ex>Program 1, Program 2</ex></ph>
The installation will not proceed if you choose to cancel.
            </message>
          </messages>
        </release>
      </grit>'''), dir='.')

    # test rig
    class DummyOpts(object):
      verbose = False
      extra_verbose = False
    tool.o = DummyOpts()
    result = tool.Process(rc_text, '.\resource.rc')
    self.failUnless(
      result.children[2].children[2].children[0].attrs['desc'] == '')
    self.failUnless(
      result.children[2].children[2].children[0].children[0].attrs['name'] == 'USERNAME')
    self.failUnless(
      result.children[2].children[2].children[1].attrs['desc'] == 'The other description')
    self.failUnless(
      result.children[2].children[2].children[1].attrs['meaning'] == '')
    self.failUnless(
      result.children[2].children[2].children[1].children[0].attrs['name'] == 'USERNAME')
    self.failUnless(
      result.children[2].children[2].children[1].children[1].attrs['name'] == 'ADMINNAME')
    self.failUnless(
      result.children[2].children[2].children[2].children[0].attrs['name'] == 'LIST_OF_PROGRAMS')

  def testRunOutput(self):
    """Verify basic correct Run behavior."""
    tool = rc2grd.Rc2Grd()
    class DummyOpts(object):
      verbose = False
      extra_verbose = False
    with util.TempDir({}) as output_dir:
      rcfile = os.path.join(output_dir.GetPath(), 'foo.rc')
      open(rcfile, 'w').close()
      self.assertIsNone(tool.Run(DummyOpts(), [rcfile]))
      self.assertTrue(os.path.exists(os.path.join(output_dir.GetPath(), 'foo.grd')))

  def testMissingOutput(self):
    """Verify failure with no args."""
    tool = rc2grd.Rc2Grd()
    class DummyOpts(object):
      verbose = False
      extra_verbose = False
    ret = tool.Run(DummyOpts(), [])
    self.assertIsNotNone(ret)
    self.assertGreater(ret, 0)


if __name__ == '__main__':
  unittest.main()
