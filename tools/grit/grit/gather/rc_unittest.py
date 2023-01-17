#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.gather.rc'''

import io
import os
import sys
import unittest

if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

from grit.gather import rc
from grit import util


class RcUnittest(unittest.TestCase):

  part_we_want = '''IDC_KLONKACC ACCELERATORS
BEGIN
    "?",            IDM_ABOUT,              ASCII,  ALT
    "/",            IDM_ABOUT,              ASCII,  ALT
END'''

  def testSectionFromFile(self):
    buf = '''IDC_SOMETHINGELSE BINGO
BEGIN
    BLA BLA
    BLA BLA
END
%s

IDC_KLONK BINGOBONGO
BEGIN
  HONGO KONGO
END
''' % self.part_we_want

    f = io.StringIO(buf)

    out = rc.Section(f, 'IDC_KLONKACC')
    out.ReadSection()
    self.assertTrue(out.GetText() == self.part_we_want)

    out = rc.Section(util.PathFromRoot(r'grit/testdata/klonk.rc'),
                     'IDC_KLONKACC',
                     encoding='utf-16')
    out.ReadSection()
    out_text = out.GetText().replace('\t', '')
    out_text = out_text.replace(' ', '')
    self.part_we_want = self.part_we_want.replace(' ', '')
    self.assertTrue(out_text.strip() == self.part_we_want.strip())


  def testDialog(self):
    dlg = rc.Dialog(
        io.StringIO('''IDD_ABOUTBOX DIALOGEX 22, 17, 230, 75
STYLE DS_SETFONT | DS_MODALFRAME | WS_CAPTION | WS_SYSMENU
CAPTION "About"
FONT 8, "System", 0, 0, 0x0
BEGIN
    ICON            IDI_KLONK,IDC_MYICON,14,9,20,20
    LTEXT           "klonk Version ""yibbee"" 1.0",IDC_STATIC,49,10,119,8,
                    SS_NOPREFIX
    LTEXT           "Copyright (C) 2005",IDC_STATIC,49,20,119,8
    DEFPUSHBUTTON   "OK",IDOK,195,6,30,11,WS_GROUP
    CONTROL         "Jack ""Black"" Daniels",IDC_RADIO1,"Button",
                    BS_AUTORADIOBUTTON,46,51,84,10
    // try a line where the ID is on the continuation line
    LTEXT           "blablablabla blablabla blablablablablablablabla blablabla",
                    ID_SMURF, whatever...
END
'''), 'IDD_ABOUTBOX')
    dlg.Parse()
    self.assertTrue(len(dlg.GetTextualIds()) == 7)
    self.assertTrue(len(dlg.GetCliques()) == 6)
    self.assertTrue(dlg.GetCliques()[1].GetMessage().GetRealContent() ==
                    'klonk Version "yibbee" 1.0')

    transl = dlg.Translate('en')
    self.assertTrue(transl.strip() == dlg.GetText().strip())

  def testAlternateSkeleton(self):
    dlg = rc.Dialog(
        io.StringIO('''IDD_ABOUTBOX DIALOGEX 22, 17, 230, 75
STYLE DS_SETFONT | DS_MODALFRAME | WS_CAPTION | WS_SYSMENU
CAPTION "About"
FONT 8, "System", 0, 0, 0x0
BEGIN
    LTEXT           "Yipee skippy",IDC_STATIC,49,10,119,8,
                    SS_NOPREFIX
END
'''), 'IDD_ABOUTBOX')
    dlg.Parse()

    alt_dlg = rc.Dialog(
        io.StringIO('''IDD_ABOUTBOX DIALOGEX 040704, 17, 230, 75
STYLE DS_SETFONT | DS_MODALFRAME | WS_CAPTION | WS_SYSMENU
CAPTION "XXXXXXXXX"
FONT 8, "System", 0, 0, 0x0
BEGIN
    LTEXT           "XXXXXXXXXXXXXXXXX",IDC_STATIC,110978,10,119,8,
                    SS_NOPREFIX
END
'''), 'IDD_ABOUTBOX')
    alt_dlg.Parse()

    transl = dlg.Translate('en', skeleton_gatherer=alt_dlg)
    self.assertTrue(transl.count('040704') and
                    transl.count('110978'))
    self.assertTrue(transl.count('Yipee skippy'))

  def testMenu(self):
    menu = rc.Menu(
        io.StringIO('''IDC_KLONK MENU
BEGIN
    POPUP "&File """
    BEGIN
        MENUITEM "E&xit",                       IDM_EXIT
        MENUITEM "This be ""Klonk"" me like",   ID_FILE_THISBE
        POPUP "gonk"
        BEGIN
            MENUITEM "Klonk && is ""good""",           ID_GONK_KLONKIS
        END
        MENUITEM "This is a very long menu caption to try to see if we can make the ID go to a continuation line, blablabla blablabla bla blabla blablabla blablabla blablabla blablabla...",
                                        ID_FILE_THISISAVERYLONGMENUCAPTIONTOTRYTOSEEIFWECANMAKETHEIDGOTOACONTINUATIONLINE
    END
    POPUP "&Help"
    BEGIN
        MENUITEM "&About ...",                  IDM_ABOUT
    END
END'''), 'IDC_KLONK')

    menu.Parse()
    self.assertTrue(len(menu.GetTextualIds()) == 6)
    self.assertTrue(len(menu.GetCliques()) == 1)
    self.assertTrue(len(menu.GetCliques()[0].GetMessage().GetPlaceholders()) ==
                    9)

    transl = menu.Translate('en')
    self.assertTrue(transl.strip() == menu.GetText().strip())

  def testVersion(self):
    version = rc.Version(
        io.StringIO('''
VS_VERSION_INFO VERSIONINFO
 FILEVERSION 1,0,0,1
 PRODUCTVERSION 1,0,0,1
 FILEFLAGSMASK 0x3fL
#ifdef _DEBUG
 FILEFLAGS 0x1L
#else
 FILEFLAGS 0x0L
#endif
 FILEOS 0x4L
 FILETYPE 0x2L
 FILESUBTYPE 0x0L
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904e4"
        BEGIN
            VALUE "CompanyName", "TODO: <Company name>"
            VALUE "FileDescription", "TODO: <File description>"
            VALUE "FileVersion", "1.0.0.1"
            VALUE "LegalCopyright", "TODO: (c) <Company name>.  All rights reserved."
            VALUE "InternalName", "res_format_test.dll"
            VALUE "OriginalFilename", "res_format_test.dll"
            VALUE "ProductName", "TODO: <Product name>"
            VALUE "ProductVersion", "1.0.0.1"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x409, 1252
    END
END
'''.strip()), 'VS_VERSION_INFO')
    version.Parse()
    self.assertTrue(len(version.GetTextualIds()) == 1)
    self.assertTrue(len(version.GetCliques()) == 4)

    transl = version.Translate('en')
    self.assertTrue(transl.strip() == version.GetText().strip())


  def testRegressionDialogBox(self):
    dialog = rc.Dialog(
        io.StringIO('''
IDD_SIDEBAR_WEATHER_PANEL_PROPPAGE DIALOGEX 0, 0, 205, 157
STYLE DS_SETFONT | DS_FIXEDSYS | WS_CHILD
FONT 8, "MS Shell Dlg", 400, 0, 0x1
BEGIN
    EDITTEXT        IDC_SIDEBAR_WEATHER_NEW_CITY,3,27,112,14,ES_AUTOHSCROLL
    DEFPUSHBUTTON   "Add Location",IDC_SIDEBAR_WEATHER_ADD,119,27,50,14
    LISTBOX         IDC_SIDEBAR_WEATHER_CURR_CITIES,3,48,127,89,
                    LBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_TABSTOP
    PUSHBUTTON      "Move Up",IDC_SIDEBAR_WEATHER_MOVE_UP,134,104,50,14
    PUSHBUTTON      "Move Down",IDC_SIDEBAR_WEATHER_MOVE_DOWN,134,121,50,14
    PUSHBUTTON      "Remove",IDC_SIDEBAR_WEATHER_DELETE,134,48,50,14
    LTEXT           "To see current weather conditions and forecasts in the USA, enter the zip code (example: 94043) or city and state (example: Mountain View, CA).",
                    IDC_STATIC,3,0,199,25
    CONTROL         "Fahrenheit",IDC_SIDEBAR_WEATHER_FAHRENHEIT,"Button",
                    BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP,3,144,51,10
    CONTROL         "Celsius",IDC_SIDEBAR_WEATHER_CELSIUS,"Button",
                    BS_AUTORADIOBUTTON,57,144,38,10
END'''.strip()), 'IDD_SIDEBAR_WEATHER_PANEL_PROPPAGE')
    dialog.Parse()
    self.assertTrue(len(dialog.GetTextualIds()) == 10)


  def testRegressionDialogBox2(self):
    dialog = rc.Dialog(
        io.StringIO('''
IDD_SIDEBAR_EMAIL_PANEL_PROPPAGE DIALOG DISCARDABLE 0, 0, 264, 220
STYLE WS_CHILD
FONT 8, "MS Shell Dlg"
BEGIN
    GROUPBOX        "Email Filters",IDC_STATIC,7,3,250,190
    LTEXT           "Click Add Filter to create the email filter.",IDC_STATIC,16,41,130,9
    PUSHBUTTON      "Add Filter...",IDC_SIDEBAR_EMAIL_ADD_FILTER,196,38,50,14
    PUSHBUTTON      "Remove",IDC_SIDEBAR_EMAIL_REMOVE,196,174,50,14
    PUSHBUTTON      "", IDC_SIDEBAR_EMAIL_HIDDEN, 200, 178, 5, 5, NOT WS_VISIBLE
    LISTBOX         IDC_SIDEBAR_EMAIL_LIST,16,60,230,108,
                    LBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_TABSTOP
    LTEXT           "You can prevent certain emails from showing up in the sidebar with a filter.",
                    IDC_STATIC,16,18,234,18
END'''.strip()), 'IDD_SIDEBAR_EMAIL_PANEL_PROPPAGE')
    dialog.Parse()
    self.assertTrue('IDC_SIDEBAR_EMAIL_HIDDEN' in dialog.GetTextualIds())


  def testRegressionMenuId(self):
    menu = rc.Menu(
        io.StringIO('''
IDR_HYPERMENU_FOLDER MENU
BEGIN
    POPUP "HyperFolder"
    BEGIN
        MENUITEM "Open Containing Folder",      IDM_OPENFOLDER
    END
END'''.strip()), 'IDR_HYPERMENU_FOLDER')
    menu.Parse()
    self.assertTrue(len(menu.GetTextualIds()) == 2)

  def testRegressionNewlines(self):
    menu = rc.Menu(
        io.StringIO('''
IDR_HYPERMENU_FOLDER MENU
BEGIN
    POPUP "Hyper\\nFolder"
    BEGIN
        MENUITEM "Open Containing Folder",      IDM_OPENFOLDER
    END
END'''.strip()), 'IDR_HYPERMENU_FOLDER')
    menu.Parse()
    transl = menu.Translate('en')
    # Shouldn't find \\n (the \n shouldn't be changed to \\n)
    self.assertTrue(transl.find('\\\\n') == -1)

  def testRegressionTabs(self):
    menu = rc.Menu(
        io.StringIO('''
IDR_HYPERMENU_FOLDER MENU
BEGIN
    POPUP "Hyper\\tFolder"
    BEGIN
        MENUITEM "Open Containing Folder",      IDM_OPENFOLDER
    END
END'''.strip()), 'IDR_HYPERMENU_FOLDER')
    menu.Parse()
    transl = menu.Translate('en')
    # Shouldn't find \\t (the \t shouldn't be changed to \\t)
    self.assertTrue(transl.find('\\\\t') == -1)

  def testEscapeUnescape(self):
    original = 'Hello "bingo"\n How\\are\\you\\n?'
    escaped = rc.Section.Escape(original)
    self.assertTrue(escaped == 'Hello ""bingo""\\n How\\\\are\\\\you\\\\n?')
    unescaped = rc.Section.UnEscape(escaped)
    self.assertTrue(unescaped == original)

  def testRegressionPathsWithSlashN(self):
    original = '..\\\\..\\\\trs\\\\res\\\\nav_first.gif'
    unescaped = rc.Section.UnEscape(original)
    self.assertTrue(unescaped == '..\\..\\trs\\res\\nav_first.gif')

  def testRegressionDialogItemsTextOnly(self):
    dialog = rc.Dialog(
        io.StringIO('''IDD_OPTIONS_SEARCH DIALOGEX 0, 0, 280, 292
STYLE DS_SETFONT | DS_MODALFRAME | DS_FIXEDSYS | DS_CENTER | WS_POPUP |
    WS_DISABLED | WS_CAPTION | WS_SYSMENU
CAPTION "Search"
FONT 8, "MS Shell Dlg", 400, 0, 0x1
BEGIN
    GROUPBOX        "Select search buttons and options",-1,7,5,266,262
    CONTROL         "",IDC_OPTIONS,"SysTreeView32",TVS_DISABLEDRAGDROP |
                    WS_BORDER | WS_TABSTOP | 0x800,16,19,248,218
    LTEXT           "Use Google site:",-1,26,248,52,8
    COMBOBOX        IDC_GOOGLE_HOME,87,245,177,256,CBS_DROPDOWNLIST |
                    WS_VSCROLL | WS_TABSTOP
    PUSHBUTTON      "Restore Defaults...",IDC_RESET,187,272,86,14
END'''), 'IDD_OPTIONS_SEARCH')
    dialog.Parse()
    translateables = [c.GetMessage().GetRealContent()
                      for c in dialog.GetCliques()]
    self.assertTrue('Select search buttons and options' in translateables)
    self.assertTrue('Use Google site:' in translateables)

  def testAccelerators(self):
    acc = rc.Accelerators(
        io.StringIO('''\
IDR_ACCELERATOR1 ACCELERATORS
BEGIN
    "^C",           ID_ACCELERATOR32770,    ASCII,  NOINVERT
    "^V",           ID_ACCELERATOR32771,    ASCII,  NOINVERT
    VK_INSERT,      ID_ACCELERATOR32772,    VIRTKEY, CONTROL, NOINVERT
END
'''), 'IDR_ACCELERATOR1')
    acc.Parse()
    self.assertTrue(len(acc.GetTextualIds()) == 4)
    self.assertTrue(len(acc.GetCliques()) == 0)

    transl = acc.Translate('en')
    self.assertTrue(transl.strip() == acc.GetText().strip())


  def testRegressionEmptyString(self):
    dlg = rc.Dialog(
        io.StringIO('''\
IDD_CONFIRM_QUIT_GD_DLG DIALOGEX 0, 0, 267, 108
STYLE DS_SETFONT | DS_MODALFRAME | DS_FIXEDSYS | DS_CENTER | WS_POPUP |
    WS_CAPTION
EXSTYLE WS_EX_TOPMOST
CAPTION "Google Desktop"
FONT 8, "MS Shell Dlg", 400, 0, 0x1
BEGIN
    DEFPUSHBUTTON   "&Yes",IDYES,82,87,50,14
    PUSHBUTTON      "&No",IDNO,136,87,50,14
    ICON            32514,IDC_STATIC,7,9,21,20
    EDITTEXT        IDC_TEXTBOX,34,7,231,60,ES_MULTILINE | ES_READONLY | NOT WS_BORDER
    CONTROL         "",
                    IDC_ENABLE_GD_AUTOSTART,"Button",BS_AUTOCHECKBOX |
                    WS_TABSTOP,33,70,231,10
END'''), 'IDD_CONFIRM_QUIT_GD_DLG')
    dlg.Parse()

    def Check():
      self.assertTrue(transl.count('IDC_ENABLE_GD_AUTOSTART'))
      self.assertTrue(transl.count('END'))

    transl = dlg.Translate('de', pseudo_if_not_available=True,
                           fallback_to_english=True)
    Check()
    transl = dlg.Translate('de', pseudo_if_not_available=True,
                           fallback_to_english=False)
    Check()
    transl = dlg.Translate('de', pseudo_if_not_available=False,
                           fallback_to_english=True)
    Check()
    transl = dlg.Translate('de', pseudo_if_not_available=False,
                           fallback_to_english=False)
    Check()
    transl = dlg.Translate('en', pseudo_if_not_available=True,
                           fallback_to_english=True)
    Check()
    transl = dlg.Translate('en', pseudo_if_not_available=True,
                           fallback_to_english=False)
    Check()
    transl = dlg.Translate('en', pseudo_if_not_available=False,
                           fallback_to_english=True)
    Check()
    transl = dlg.Translate('en', pseudo_if_not_available=False,
                           fallback_to_english=False)
    Check()


if __name__ == '__main__':
  unittest.main()
