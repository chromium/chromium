#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for node_io.FileNode'''

import os
import sys
import unittest
import io

if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

from grit.node import misc
from grit.node import node_io
from grit.node import empty
from grit import grd_reader
from grit import util


def _GetAllCliques(root_node):
  """Return all cliques in the |root_node| tree."""
  ret = []
  for node in root_node:
    ret.extend(node.GetCliques())
  return ret


class FileNodeUnittest(unittest.TestCase):
  def testGetPath(self):
    root = misc.GritNode()
    root.StartParsing('grit', None)
    root.HandleAttribute('latest_public_release', '0')
    root.HandleAttribute('current_release', '1')
    root.HandleAttribute('base_dir', r'..\resource')
    translations = empty.TranslationsNode()
    translations.StartParsing('translations', root)
    root.AddChild(translations)
    file_node = node_io.FileNode()
    file_node.StartParsing('file', translations)
    file_node.HandleAttribute('path', r'flugel\kugel.pdf')
    translations.AddChild(file_node)
    root.EndParsing()

    self.assertTrue(root.ToRealPath(file_node.GetInputPath()) ==
                    util.normpath(
                      os.path.join(r'../resource', r'flugel/kugel.pdf')))

  def VerifyCliquesContainEnglishAndFrenchAndNothingElse(self, cliques):
    self.assertEqual(2, len(cliques))
    for clique in cliques:
      self.assertEqual({'en', 'fr'}, set(clique.clique.keys()))

  def testLoadTranslations(self):
    xml = '''<?xml version="1.0" encoding="UTF-8"?>
      <grit latest_public_release="2" source_lang_id="en-US" current_release="3" base_dir=".">
        <translations>
          <file path="generated_resources_fr.xtb" lang="fr" />
        </translations>
        <release seq="3">
          <messages>
            <message name="ID_HELLO">Hello!</message>
            <message name="ID_HELLO_USER">Hello <ph name="USERNAME">%s<ex>Joi</ex></ph></message>
          </messages>
        </release>
      </grit>'''
    grd = grd_reader.Parse(io.StringIO(xml), util.PathFromRoot('grit/testdata'))
    grd.SetOutputLanguage('en')
    grd.RunGatherers()
    self.VerifyCliquesContainEnglishAndFrenchAndNothingElse(_GetAllCliques(grd))

  def testIffyness(self):
    grd = grd_reader.Parse(
        io.StringIO('''<?xml version="1.0" encoding="UTF-8"?>
      <grit latest_public_release="2" source_lang_id="en-US" current_release="3" base_dir=".">
        <translations>
          <if expr="lang == 'fr'">
            <file path="generated_resources_fr.xtb" lang="fr" />
          </if>
        </translations>
        <release seq="3">
          <messages>
            <message name="ID_HELLO">Hello!</message>
            <message name="ID_HELLO_USER">Hello <ph name="USERNAME">%s<ex>Joi</ex></ph></message>
          </messages>
        </release>
      </grit>'''), util.PathFromRoot('grit/testdata'))
    grd.SetOutputLanguage('en')
    grd.RunGatherers()
    cliques = _GetAllCliques(grd)
    self.assertEqual(2, len(cliques))
    for clique in cliques:
      self.assertEqual({'en'}, set(clique.clique.keys()))

    grd.SetOutputLanguage('fr')
    grd.RunGatherers()
    self.VerifyCliquesContainEnglishAndFrenchAndNothingElse(_GetAllCliques(grd))

  def testConditionalLoadTranslations(self):
    xml = '''<?xml version="1.0" encoding="UTF-8"?>
      <grit latest_public_release="2" source_lang_id="en-US" current_release="3"
            base_dir=".">
        <translations>
          <if expr="True">
            <file path="generated_resources_fr.xtb" lang="fr" />
          </if>
          <if expr="False">
            <file path="no_such_file.xtb" lang="de" />
          </if>
        </translations>
        <release seq="3">
          <messages>
            <message name="ID_HELLO">Hello!</message>
            <message name="ID_HELLO_USER">Hello <ph name="USERNAME">%s<ex>
              Joi</ex></ph></message>
          </messages>
        </release>
      </grit>'''
    grd = grd_reader.Parse(io.StringIO(xml), util.PathFromRoot('grit/testdata'))
    grd.SetOutputLanguage('en')
    grd.RunGatherers()
    self.VerifyCliquesContainEnglishAndFrenchAndNothingElse(_GetAllCliques(grd))

  def testConditionalOutput(self):
    xml = '''<?xml version="1.0" encoding="UTF-8"?>
      <grit latest_public_release="2" source_lang_id="en-US" current_release="3"
            base_dir=".">
        <outputs>
          <output filename="resource.h" type="rc_header" />
          <output filename="en/generated_resources.rc" type="rc_all"
                  lang="en" />
          <if expr="pp_if('NOT_TRUE')">
            <output filename="de/generated_resources.rc" type="rc_all"
                    lang="de" />
          </if>
        </outputs>
        <release seq="3">
          <messages>
            <message name="ID_HELLO">Hello!</message>
          </messages>
        </release>
      </grit>'''
    grd = grd_reader.Parse(io.StringIO(xml),
                           util.PathFromRoot('grit/test/data'),
                           defines={})
    grd.SetOutputLanguage('en')
    grd.RunGatherers()
    outputs = grd.GetChildrenOfType(node_io.OutputNode)
    active = set(grd.ActiveDescendants())
    self.assertTrue(outputs[0] in active)
    self.assertTrue(outputs[0].GetType() == 'rc_header')
    self.assertTrue(outputs[1] in active)
    self.assertTrue(outputs[1].GetType() == 'rc_all')
    self.assertTrue(outputs[2] not in active)
    self.assertTrue(outputs[2].GetType() == 'rc_all')

  # Verify that 'iw' and 'no' language codes in xtb files are mapped to 'he' and
  # 'nb'.
  def testLangCodeMapping(self):
    grd = grd_reader.Parse(
        io.StringIO('''<?xml version="1.0" encoding="UTF-8"?>
      <grit latest_public_release="2" source_lang_id="en-US" current_release="3" base_dir=".">
        <translations>
          <file path="generated_resources_no.xtb" lang="nb" />
          <file path="generated_resources_iw.xtb" lang="he" />
        </translations>
        <release seq="3">
          <messages></messages>
        </release>
      </grit>'''), util.PathFromRoot('grit/testdata'))
    grd.SetOutputLanguage('en')
    grd.RunGatherers()
    self.assertEqual([], _GetAllCliques(grd))


if __name__ == '__main__':
  unittest.main()
