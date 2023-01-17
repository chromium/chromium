#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.tclib'''

import sys
import os.path
import unittest

if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

from grit import tclib

from grit import exception
import grit.extern.tclib


class TclibUnittest(unittest.TestCase):
  def testInit(self):
    msg = tclib.Message(text='Hello Earthlings',
                        description='Greetings\n\t      message')
    self.assertEqual(msg.GetPresentableContent(), 'Hello Earthlings')
    self.assertTrue(isinstance(msg.GetPresentableContent(), str))
    self.assertEqual(msg.GetDescription(), 'Greetings message')

  def testGetAttr(self):
    msg = tclib.Message()
    msg.AppendText('Hello')  # Tests __getattr__
    self.assertTrue(msg.GetPresentableContent() == 'Hello')
    self.assertTrue(isinstance(msg.GetPresentableContent(), str))

  def testAll(self):
    text = 'Howdie USERNAME'
    phs = [tclib.Placeholder('USERNAME', '%s', 'Joi')]
    msg = tclib.Message(text=text, placeholders=phs)
    self.assertTrue(msg.GetPresentableContent() == 'Howdie USERNAME')

    trans = tclib.Translation(text=text, placeholders=phs)
    self.assertTrue(trans.GetPresentableContent() == 'Howdie USERNAME')
    self.assertTrue(isinstance(trans.GetPresentableContent(), str))

  def testUnicodeReturn(self):
    text = '\u00fe'
    msg = tclib.Message(text=text)
    self.assertTrue(msg.GetPresentableContent() == text)
    from_list = msg.GetContent()[0]
    self.assertTrue(from_list == text)

  def testRegressionTranslationInherited(self):
    '''Regression tests a bug that was caused by grit.tclib.Translation
    inheriting from the translation console's Translation object
    instead of only owning an instance of it.
    '''
    msg = tclib.Message(text="BLA1\r\nFrom: BLA2 \u00fe BLA3",
                        placeholders=[
                          tclib.Placeholder('BLA1', '%s', '%s'),
                          tclib.Placeholder('BLA2', '%s', '%s'),
                          tclib.Placeholder('BLA3', '%s', '%s')])
    transl = tclib.Translation(text=msg.GetPresentableContent(),
                               placeholders=msg.GetPlaceholders())
    content = transl.GetContent()
    self.assertTrue(isinstance(content[3], str))

  def testFingerprint(self):
    # This has Windows line endings.  That is on purpose.
    id = grit.extern.tclib.GenerateMessageId(
      'Google Desktop for Enterprise\r\n'
      'All Rights Reserved\r\n'
      '\r\n'
      '---------\r\n'
      'Contents\r\n'
      '---------\r\n'
      'This distribution contains the following files:\r\n'
      '\r\n'
      'GoogleDesktopSetup.msi - Installation and setup program\r\n'
      'GoogleDesktop.adm - Group Policy administrative template file\r\n'
      'AdminGuide.pdf - Google Desktop for Enterprise administrative guide\r\n'
      '\r\n'
      '\r\n'
      '--------------\r\n'
      'Documentation\r\n'
      '--------------\r\n'
      'Full documentation and installation instructions are in the \r\n'
      'administrative guide, and also online at \r\n'
      'http://desktop.google.com/enterprise/adminguide.html.\r\n'
      '\r\n'
      '\r\n'
      '------------------------\r\n'
      'IBM Lotus Notes Plug-In\r\n'
      '------------------------\r\n'
      'The Lotus Notes plug-in is included in the release of Google \r\n'
      'Desktop for Enterprise. The IBM Lotus Notes Plug-in for Google \r\n'
      'Desktop indexes mail, calendar, task, contact and journal \r\n'
      'documents from Notes.  Discussion documents including those from \r\n'
      'the discussion and team room templates can also be indexed by \r\n'
      'selecting an option from the preferences.  Once indexed, this data\r\n'
      'will be returned in Google Desktop searches.  The corresponding\r\n'
      'document can be opened in Lotus Notes from the Google Desktop \r\n'
      'results page.\r\n'
      '\r\n'
      'Install: The plug-in will install automatically during the Google \r\n'
      'Desktop setup process if Lotus Notes is already installed.  Lotus \r\n'
      'Notes must not be running in order for the install to occur.  \r\n'
      '\r\n'
      'Preferences: Preferences and selection of databases to index are\r\n'
      'set in the \'Google Desktop for Notes\' dialog reached through the \r\n'
      '\'Actions\' menu.\r\n'
      '\r\n'
      'Reindexing: Selecting \'Reindex all databases\' will index all the \r\n'
      'documents in each database again.\r\n'
      '\r\n'
      '\r\n'
      'Notes Plug-in Known Issues\r\n'
      '---------------------------\r\n'
      '\r\n'
      'If the \'Google Desktop for Notes\' item is not available from the \r\n'
      'Lotus Notes Actions menu, then installation was not successful. \r\n'
      'Installation consists of writing one file, notesgdsplugin.dll, to \r\n'
      'the Notes application directory and a setting to the notes.ini \r\n'
      'configuration file.  The most likely cause of an unsuccessful \r\n'
      'installation is that the installer was not able to locate the \r\n'
      'notes.ini file. Installation will complete if the user closes Notes\r\n'
      'and manually adds the following setting to this file on a new line:\r\n'
      'AddinMenus=notegdsplugin.dll\r\n'
      '\r\n'
      'If the notesgdsplugin.dll file is not in the application directory\r\n'
      r'(e.g., C:\Program Files\Lotus\Notes) after Google Desktop \r\n'
      'installation, it is likely that Notes was not installed correctly. \r\n'
      '\r\n'
      'Only local databases can be indexed.  If they can be determined, \r\n'
      'the user\'s local mail file and address book will be included in the\r\n'
      'list automatically.  Mail archives and other databases must be \r\n'
      'added with the \'Add\' button.\r\n'
      '\r\n'
      'Some users may experience performance issues during the initial \r\n'
      'indexing of a database.  The \'Perform the initial index of a \r\n'
      'database only when I\'m idle\' option will limit the indexing process\r\n'
      'to times when the user is not using the machine. If this does not \r\n'
      'alleviate the problem or the user would like to continually index \r\n'
      'but just do so more slowly or quickly, the GoogleWaitTime notes.ini\r\n'
      'value can be set. Increasing the GoogleWaitTime value will slow \r\n'
      'down the indexing process, and lowering the value will speed it up.\r\n'
      'A value of zero causes the fastest possible indexing.  Removing the\r\n'
      'ini parameter altogether returns it to the default (20).\r\n'
      '\r\n'
      'Crashes have been known to occur with certain types of history \r\n'
      'bookmarks.  If the Notes client seems to crash randomly, try \r\n'
      'disabling the \'Index note history\' option.  If it crashes before,\r\n'
      'you can get to the preferences, add the following line to your \r\n'
      'notes.ini file:\r\n'
      'GDSNoIndexHistory=1\r\n')
    self.assertEqual(id, '7660964495923572726')

  def testPlaceholderNameChecking(self):
    try:
      ph = tclib.Placeholder('BINGO BONGO', 'bla', 'bla')
      raise Exception("We shouldn't get here")
    except exception.InvalidPlaceholderName:
      pass  # Expect exception to be thrown because presentation contained space

  def testTagsWithCommonSubstring(self):
    word = 'ABCDEFGHIJ'
    text = ' '.join([word[:i] for i in range(1, 11)])
    phs = [tclib.Placeholder(word[:i], str(i), str(i)) for i in range(1, 11)]
    try:
      msg = tclib.Message(text=text, placeholders=phs)
      self.assertTrue(msg.GetRealContent() == '1 2 3 4 5 6 7 8 9 10')
    except:
      self.fail('tclib.Message() should handle placeholders that are '
                'substrings of each other')

if __name__ == '__main__':
  unittest.main()
