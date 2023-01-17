#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.pseudo'''


import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

import unittest

from grit import pseudo
from grit import tclib


class PseudoUnittest(unittest.TestCase):
  def testVowelMapping(self):
    self.assertTrue(pseudo.MapVowels('abebibobuby') ==
                    '\u00e5b\u00e9b\u00efb\u00f4b\u00fcb\u00fd')
    self.assertTrue(pseudo.MapVowels('ABEBIBOBUBY') ==
                    '\u00c5B\u00c9B\u00cfB\u00d4B\u00dcB\u00dd')

  def testPseudoString(self):
    out = pseudo.PseudoString('hello')
    self.assertTrue(out == pseudo.MapVowels('hePelloPo', True))

  def testConsecutiveVowels(self):
    out = pseudo.PseudoString("beautiful weather, ain't it?")
    self.assertTrue(out == pseudo.MapVowels(
      "beauPeautiPifuPul weaPeathePer, aiPain't iPit?", 1))

  def testCapitals(self):
    out = pseudo.PseudoString("HOWDIE DOODIE, DR. JONES")
    self.assertTrue(out == pseudo.MapVowels(
      "HOPOWDIEPIE DOOPOODIEPIE, DR. JOPONEPES", 1))

  def testPseudoMessage(self):
    msg = tclib.Message(text='Hello USERNAME, how are you?',
                        placeholders=[
                          tclib.Placeholder('USERNAME', '%s', 'Joi')])
    trans = pseudo.PseudoMessage(msg)
    # TODO(joi) It would be nicer if 'you' -> 'youPou' instead of
    # 'you' -> 'youPyou' and if we handled the silent e in 'are'
    self.assertTrue(trans.GetPresentableContent() ==
                    pseudo.MapVowels(
                      'HePelloPo USERNAME, hoPow aParePe youPyou?', 1))


if __name__ == '__main__':
  unittest.main()
