#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for the admin template gatherer.'''

from __future__ import print_function

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

import unittest

from six import StringIO

from grit.gather import admin_template
from grit import util
from grit import grd_reader
from grit import grit_runner
from grit.tool import build


class AdmGathererUnittest(unittest.TestCase):
  def testParsingAndTranslating(self):
    pseudofile = StringIO(
      'bingo bongo\n'
      'ding dong\n'
      '[strings] \n'
      'whatcha="bingo bongo"\n'
      'gotcha = "bingolabongola "the wise" fingulafongula" \n')
    gatherer = admin_template.AdmGatherer(pseudofile)
    gatherer.Parse()
    self.failUnless(len(gatherer.GetCliques()) == 2)
    self.failUnless(gatherer.GetCliques()[1].GetMessage().GetRealContent() ==
                    'bingolabongola "the wise" fingulafongula')

    translation = gatherer.Translate('en')
    self.failUnless(translation == gatherer.GetText().strip())

  def testErrorHandling(self):
    pseudofile = StringIO(
      'bingo bongo\n'
      'ding dong\n'
      'whatcha="bingo bongo"\n'
      'gotcha = "bingolabongola "the wise" fingulafongula" \n')
    gatherer = admin_template.AdmGatherer(pseudofile)
    self.assertRaises(admin_template.MalformedAdminTemplateException,
                      gatherer.Parse)

  _TRANSLATABLES_FROM_FILE = (
    'Google', 'Google Desktop', 'Preferences',
    'Controls Google Desktop preferences',
    'Indexing and Capture Control',
    'Controls what files, web pages, and other content will be indexed by Google Desktop.',
    'Prevent indexing of email',
    # there are lots more but we don't check any further
  )

  def VerifyCliquesFromAdmFile(self, cliques):
    self.failUnless(len(cliques) > 20)
    for clique, expected in zip(cliques, self._TRANSLATABLES_FROM_FILE):
      text = clique.GetMessage().GetRealContent()
      self.failUnless(text == expected)

  def testFromFile(self):
    fname = util.PathFromRoot('grit/testdata/GoogleDesktop.adm')
    gatherer = admin_template.AdmGatherer(fname)
    gatherer.Parse()
    cliques = gatherer.GetCliques()
    self.VerifyCliquesFromAdmFile(cliques)

  def MakeGrd(self):
    grd = grd_reader.Parse(StringIO('''<?xml version="1.0" encoding="UTF-8"?>
      <grit latest_public_release="2" source_lang_id="en-US" current_release="3">
        <release seq="3">
          <structures>
            <structure type="admin_template" name="IDAT_GOOGLE_DESKTOP_SEARCH"
              file="GoogleDesktop.adm" exclude_from_rc="true" />
            <structure type="txt" name="BINGOBONGO"
              file="README.txt" exclude_from_rc="true" />
          </structures>
        </release>
        <outputs>
          <output filename="de_res.rc" type="rc_all" lang="de" />
        </outputs>
      </grit>'''), util.PathFromRoot('grit/testdata'))
    grd.SetOutputLanguage('en')
    grd.RunGatherers()
    return grd

  def testInGrd(self):
    grd = self.MakeGrd()
    cliques = grd.children[0].children[0].children[0].GetCliques()
    self.VerifyCliquesFromAdmFile(cliques)

  def testFileIsOutput(self):
    grd = self.MakeGrd()
    dirname = util.TempDir({})
    try:
      tool = build.RcBuilder()
      tool.o = grit_runner.Options()
      tool.output_directory = dirname.GetPath()
      tool.res = grd
      tool.Process()

      self.failUnless(os.path.isfile(dirname.GetPath('de_GoogleDesktop.adm')))
      self.failUnless(os.path.isfile(dirname.GetPath('de_README.txt')))
    finally:
      dirname.CleanUp()

if __name__ == '__main__':
  unittest.main()
