# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Supports making amessage from a text file.
'''

from __future__ import print_function

from grit.gather import interface
from grit import tclib


class TxtFile(interface.GathererBase):
  '''A text file gatherer.  Very simple, all text from the file becomes a
  single clique.
  '''

  def Parse(self):
    self.text_ = self._LoadInputFile()
    self.clique_ = self.uberclique.MakeClique(tclib.Message(text=self.text_))

  def GetText(self):
    '''Returns the text of what is being gathered.'''
    return self.text_

  def GetTextualIds(self):
    return [self.extkey]

  def GetCliques(self):
    '''Returns the MessageClique objects for all translateable portions.'''
    return [self.clique_]

  def Translate(self, lang, pseudo_if_not_available=True,
                skeleton_gatherer=None, fallback_to_english=False):
    return self.clique_.MessageForLanguage(lang,
                                           pseudo_if_not_available,
                                           fallback_to_english).GetRealContent()
