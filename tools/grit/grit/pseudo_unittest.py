#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.pseudo'''

from __future__ import print_function

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

import unittest

from grit import pseudo
from grit import tclib


class PseudoUnittest(unittest.TestCase):
  def testVowelMapping(self):
    self.failUnless(pseudo.MapVowels('abebibobuby') ==
                    u'\u00e5b\u00e9b\u00efb\u00f4b\u00fcb\u00fd')
    self.failUnless(pseudo.MapVowels('ABEBIBOBUBY') ==
                    u'\u00c5B\u00c9B\u00cfB\u00d4B\u00dcB\u00dd')

  def testPseudoString(self):
    out = pseudo.PseudoString('hello')
    self.failUnless(out == pseudo.MapVowels(u'hePelloPo', True))

  def testConsecutiveVowels(self):
    out = pseudo.PseudoString("beautiful weather, ain't it?")
    self.failUnless(out == pseudo.MapVowels(
      u"beauPeautiPifuPul weaPeathePer, aiPain't iPit?", 1))

  def testCapitals(self):
    out = pseudo.PseudoString("HOWDIE DOODIE, DR. JONES")
    self.failUnless(out == pseudo.MapVowels(
      u"HOPOWDIEPIE DOOPOODIEPIE, DR. JOPONEPES", 1))

  def testPseudoMessage(self):
    msg = tclib.Message(text='Hello USERNAME, how are you?',
                        placeholders=[
                          tclib.Placeholder('USERNAME', '%s', 'Joi')])
    trans = pseudo.PseudoMessage(msg)
    # TODO(joi) It would be nicer if 'you' -> 'youPou' instead of
    # 'you' -> 'youPyou' and if we handled the silent e in 'are'
    self.failUnless(trans.GetPresentableContent() ==
                    pseudo.MapVowels(
                      u'HePelloPo USERNAME, hoPow aParePe youPyou?', 1))


if __name__ == '__main__':
  unittest.main()
