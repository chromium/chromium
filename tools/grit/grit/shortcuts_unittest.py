# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.shortcuts
'''

from __future__ import print_function

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

import unittest

from six import StringIO

from grit import shortcuts
from grit import clique
from grit import tclib
from grit.gather import rc

class ShortcutsUnittest(unittest.TestCase):

  def setUp(self):
    self.uq = clique.UberClique()

  def testFunctionality(self):
    c = self.uq.MakeClique(tclib.Message(text="Hello &there"))
    c.AddToShortcutGroup('group_name')
    c = self.uq.MakeClique(tclib.Message(text="Howdie &there partner"))
    c.AddToShortcutGroup('group_name')

    warnings = shortcuts.GenerateDuplicateShortcutsWarnings(self.uq, 'PROJECT')
    self.failUnless(warnings)

  def testAmpersandEscaping(self):
    c = self.uq.MakeClique(tclib.Message(text="Hello &there"))
    c.AddToShortcutGroup('group_name')
    c = self.uq.MakeClique(tclib.Message(text="S&&T are the &letters S and T"))
    c.AddToShortcutGroup('group_name')

    warnings = shortcuts.GenerateDuplicateShortcutsWarnings(self.uq, 'PROJECT')
    self.failUnless(len(warnings) == 0)

  def testDialog(self):
    dlg = rc.Dialog(StringIO('''\
IDD_SIDEBAR_RSS_PANEL_PROPPAGE DIALOGEX 0, 0, 239, 221
STYLE DS_SETFONT | DS_FIXEDSYS | WS_CHILD
FONT 8, "MS Shell Dlg", 400, 0, 0x1
BEGIN
    PUSHBUTTON      "Add &URL",IDC_SIDEBAR_RSS_ADD_URL,182,53,57,14
    EDITTEXT        IDC_SIDEBAR_RSS_NEW_URL,0,53,178,15,ES_AUTOHSCROLL
    PUSHBUTTON      "&Remove",IDC_SIDEBAR_RSS_REMOVE,183,200,56,14
    PUSHBUTTON      "&Edit",IDC_SIDEBAR_RSS_EDIT,123,200,56,14
    CONTROL         "&Automatically add commonly viewed clips",
                    IDC_SIDEBAR_RSS_AUTO_ADD,"Button",BS_AUTOCHECKBOX |
                    BS_MULTILINE | WS_TABSTOP,0,200,120,17
    PUSHBUTTON      "",IDC_SIDEBAR_RSS_HIDDEN,179,208,6,6,NOT WS_VISIBLE
    LTEXT           "You can display clips from blogs, news sites, and other online sources.",
                    IDC_STATIC,0,0,239,10
    LISTBOX         IDC_SIDEBAR_DISPLAYED_FEED_LIST,0,69,239,127,LBS_SORT |
                    LBS_OWNERDRAWFIXED | LBS_HASSTRINGS |
                    LBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_HSCROLL |
                    WS_TABSTOP
    LTEXT           "Add a clip from a recently viewed website by clicking Add Recent Clips.",
                    IDC_STATIC,0,13,141,19
    LTEXT           "Or, if you know a site supports RSS or Atom, you can enter the RSS or Atom URL below and add it to your list of Web Clips.",
                    IDC_STATIC,0,33,239,18
    PUSHBUTTON      "Add Recent &Clips (10)...",
                    IDC_SIDEBAR_RSS_ADD_RECENT_CLIPS,146,14,93,14
END'''), 'IDD_SIDEBAR_RSS_PANEL_PROPPAGE')
    dlg.SetUberClique(self.uq)
    dlg.Parse()

    warnings = shortcuts.GenerateDuplicateShortcutsWarnings(self.uq, 'PROJECT')
    self.failUnless(len(warnings) == 0)

