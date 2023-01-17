#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.node.custom.filename'''


import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../../..'))

import unittest
from grit.node.custom import filename
from grit import clique
from grit import tclib


class WindowsFilenameUnittest(unittest.TestCase):

  def testValidate(self):
    factory = clique.UberClique()
    msg = tclib.Message(text='Bingo bongo')
    c = factory.MakeClique(msg)
    c.SetCustomType(filename.WindowsFilename())
    translation = tclib.Translation(id=msg.GetId(), text='Bilingo bolongo:')
    c.AddTranslation(translation, 'fr')
    self.assertTrue(c.MessageForLanguage('fr').GetRealContent() == 'Bilingo bolongo ')


if __name__ == '__main__':
  unittest.main()
