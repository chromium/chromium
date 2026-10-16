#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for node_io.FileNode'''

import os
import sys
import tempfile
import unittest
from unittest import mock
import io

if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

from grit.node import misc
from grit.node import node_io
from grit.node import empty
from grit import constants
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

    self.assertTrue(
      root.ToRealPath(file_node.GetInputPath())
      == util.normpath(os.path.join(r'../resource', r'flugel/kugel.pdf'))
    )

  def VerifyCliquesContainEnglishAndFrenchAndNothingElse(self, cliques):
    self.assertEqual(2, len(cliques))
    for clique in cliques:
      self.assertEqual(
        {('en', constants.DEFAULT_GENDER), ('fr', constants.DEFAULT_GENDER)},
        set(clique.clique.keys()),
      )

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

  @unittest.skipUnless(hasattr(os, 'fork'), 'Requires os.fork()')
  def testLoadTranslationsConcurrently(self):
    xml = '''<?xml version="1.0" encoding="UTF-8"?>
      <grit latest_public_release="2" source_lang_id="en-US" current_release="3" base_dir=".">
        <translations>
          <file path="generated_resources_fr.xtb" lang="fr" />
          <file path="generated_resources_iw.xtb" lang="iw" />
          <file path="generated_resources_no.xtb" lang="no" />
        </translations>
        <release seq="3">
          <messages>
            <message name="ID_HELLO">Hello!</message>
            <message name="ID_HELLO_USER">Hello <ph name="USERNAME">%s<ex>Joi</ex></ph></message>
          </messages>
        </release>
      </grit>'''

    def LoadTranslations(concurrently):
      grd = grd_reader.Parse(
        io.StringIO(xml), util.PathFromRoot('grit/testdata')
      )
      grd.SetOutputLanguage('en')
      with (
        mock.patch.object(misc, '_CanFork', return_value=concurrently),
        mock.patch.object(
          misc.GritNode,
          '_RunPostSubstitutionGatherersConcurrently',
          autospec=True,
          side_effect=misc.GritNode._RunPostSubstitutionGatherersConcurrently,
        ) as concurrent_mock,
      ):
        grd.RunGatherers()
      self.assertEqual(concurrently, concurrent_mock.called)
      return [
        {
          key: (msg.GetPresentableContent(), msg.GetRealContent())
          for key, msg in clique.clique.items()
        }
        for clique in _GetAllCliques(grd)
      ]

    expected = LoadTranslations(concurrently=False)
    self.assertEqual(expected, LoadTranslations(concurrently=True))
    self.assertEqual(2, len(expected))
    for translations in expected:
      self.assertEqual(
        {('en', constants.DEFAULT_GENDER), ('fr', constants.DEFAULT_GENDER)},
        set(translations.keys()),
      )

  @unittest.skipUnless(hasattr(os, 'fork'), 'Requires os.fork()')
  def testLoadTranslationsConcurrentlyWithParseError(self):
    xml = '''<?xml version="1.0" encoding="UTF-8"?>
      <grit latest_public_release="2" source_lang_id="en-US" current_release="3" base_dir=".">
        <translations>
          <file path="good_fr.xtb" lang="fr" />
          <file path="bad_de.xtb" lang="de" />
        </translations>
        <release seq="3">
          <messages>
            <message name="ID_HELLO">Hello!</message>
          </messages>
        </release>
      </grit>'''
    with tempfile.TemporaryDirectory() as tmp_dir:
      with open(os.path.join(tmp_dir, 'good_fr.xtb'), 'w') as f:
        f.write('<translationbundle lang="fr"></translationbundle>')
      with open(os.path.join(tmp_dir, 'bad_de.xtb'), 'w') as f:
        f.write('<translationbundle lang="de"><translation id="1">')
      grd = grd_reader.Parse(io.StringIO(xml), tmp_dir)
      grd.SetOutputLanguage('en')
      with (
        mock.patch.object(misc, '_CanFork', return_value=True),
        mock.patch('sys.stdout', new_callable=io.StringIO),
      ):
        with self.assertRaisesRegex(RuntimeError, 'bad_de.xtb'):
          grd.RunGatherers()

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
      </grit>'''),
      util.PathFromRoot('grit/testdata'),
    )
    grd.SetOutputLanguage('en')
    grd.RunGatherers()
    cliques = _GetAllCliques(grd)
    self.assertEqual(2, len(cliques))
    for clique in cliques:
      self.assertEqual(
        {('en', constants.DEFAULT_GENDER)}, set(clique.clique.keys())
      )

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
    grd = grd_reader.Parse(
      io.StringIO(xml), util.PathFromRoot('grit/test/data'), defines={}
    )
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
      </grit>'''),
      util.PathFromRoot('grit/testdata'),
    )
    grd.SetOutputLanguage('en')
    grd.RunGatherers()
    self.assertEqual([], _GetAllCliques(grd))

  def testGenderOutput(self):
    xml = '''<?xml version="1.0" encoding="UTF-8"?>
      <grit latest_public_release="2" source_lang_id="en-US" current_release="3"
            base_dir=".">
        <outputs>
          <output filename="resource.h" type="rc_header" />
          <output filename="en/generated_resources.rc" type="rc_all"
                  lang="en" />
          <output filename="translation_en.pak" type="data_package"
                  lang="en" />
          <output filename="resources/es.pak" type="data_package"
                  lang="en" />
          <output filename="java/res/values-en/translation.xml" type="android"
                  lang="en" />
        </outputs>
        <release seq="3">
          <messages>
            <message name="ID_HELLO">Hello!</message>
          </messages>
        </release>
      </grit>'''
    grd = grd_reader.Parse(
      io.StringIO(xml),
      util.PathFromRoot('grit/testdata'),
      translate_genders=True,
    )
    grd.SetOutputLanguage('en')
    grd.RunGatherers()

    expected_files = [
      {'filename': 'resource.h', 'gender': None},
      {'filename': 'en/generated_resources.rc', 'gender': None},
      {'filename': 'translation_en.pak', 'gender': 'OTHER'},
      {'filename': 'translation_en_MASCULINE.pak', 'gender': 'MASCULINE'},
      {'filename': 'translation_en_FEMININE.pak', 'gender': 'FEMININE'},
      {'filename': 'translation_en_NEUTER.pak', 'gender': 'NEUTER'},
      {'filename': 'resources/es.pak', 'gender': 'OTHER'},
      {'filename': 'resources/es_MASCULINE.pak', 'gender': 'MASCULINE'},
      {'filename': 'resources/es_FEMININE.pak', 'gender': 'FEMININE'},
      {'filename': 'resources/es_NEUTER.pak', 'gender': 'NEUTER'},
      {'filename': 'java/res/values-en/translation.xml', 'gender': 'OTHER'},
      {
        'filename': 'java/res/values-en-MASCULINE/translation.xml',
        'gender': 'MASCULINE',
      },
      {
        'filename': 'java/res/values-en-FEMININE/translation.xml',
        'gender': 'FEMININE',
      },
      {
        'filename': 'java/res/values-en-NEUTER/translation.xml',
        'gender': 'NEUTER',
      },
    ]

    for expected_file, actual_file in zip(expected_files, grd.GetOutputFiles()):
      self.assertEqual(
        expected_file['filename'], actual_file.GetOutputFilename()
      )
      self.assertEqual(expected_file['gender'], actual_file.GetGender())


if __name__ == '__main__':
  unittest.main()
