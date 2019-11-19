#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.tool.android2grd'''

from __future__ import print_function

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

import unittest
import xml.dom.minidom

from grit import util
from grit.node import empty
from grit.node import message
from grit.node import misc
from grit.node import node_io
from grit.tool import android2grd


class Android2GrdUnittest(unittest.TestCase):

  def __Parse(self, xml_string):
    return xml.dom.minidom.parseString(xml_string).childNodes[0]

  def testCreateTclibMessage(self):
    tool = android2grd.Android2Grd()
    msg = tool.CreateTclibMessage(self.__Parse(r'''
        <string name="simple">A simple string</string>'''))
    self.assertEqual(msg.GetRealContent(), 'A simple string')
    msg = tool.CreateTclibMessage(self.__Parse(r'''
        <string name="outer_whitespace">
          Strip leading/trailing whitespace
        </string>'''))
    self.assertEqual(msg.GetRealContent(), 'Strip leading/trailing whitespace')
    msg = tool.CreateTclibMessage(self.__Parse(r'''
        <string name="inner_whitespace">Fold  multiple   spaces</string>'''))
    self.assertEqual(msg.GetRealContent(), 'Fold multiple spaces')
    msg = tool.CreateTclibMessage(self.__Parse(r'''
        <string name="escaped_spaces">Retain \n escaped\t spaces</string>'''))
    self.assertEqual(msg.GetRealContent(), 'Retain \n escaped\t spaces')
    msg = tool.CreateTclibMessage(self.__Parse(r'''
        <string name="quotes">   " Quotes  preserve
          whitespace"  but  only  for  "enclosed   elements  "
        </string>'''))
    self.assertEqual(msg.GetRealContent(), ''' Quotes  preserve
          whitespace but only for enclosed   elements  ''')
    msg = tool.CreateTclibMessage(self.__Parse(
        r'''<string name="escaped_characters">Escaped characters: \"\'\\\t\n'''
        '</string>'))
    self.assertEqual(msg.GetRealContent(), '''Escaped characters: "'\\\t\n''')
    msg = tool.CreateTclibMessage(self.__Parse(
        '<string xmlns:xliff="urn:oasis:names:tc:xliff:document:1.2" '
        'name="placeholders">'
        'Open <xliff:g id="FILENAME" example="internet.html">%s</xliff:g>?'
        '</string>'))
    self.assertEqual(msg.GetRealContent(), 'Open %s?')
    self.assertEqual(len(msg.GetPlaceholders()), 1)
    self.assertEqual(msg.GetPlaceholders()[0].presentation, 'FILENAME')
    self.assertEqual(msg.GetPlaceholders()[0].original, '%s')
    self.assertEqual(msg.GetPlaceholders()[0].example, 'internet.html')
    msg = tool.CreateTclibMessage(self.__Parse(r'''
        <string name="comment">Contains a <!-- ignore this --> comment
        </string>'''))
    self.assertEqual(msg.GetRealContent(), 'Contains a comment')

  def testIsTranslatable(self):
    tool = android2grd.Android2Grd()
    string_el = self.__Parse('<string>Hi</string>')
    self.assertTrue(tool.IsTranslatable(string_el))
    string_el = self.__Parse(
        '<string translatable="true">Hi</string>')
    self.assertTrue(tool.IsTranslatable(string_el))
    string_el = self.__Parse(
        '<string translatable="false">Hi</string>')
    self.assertFalse(tool.IsTranslatable(string_el))

  def __ParseAndroidXml(self, options = []):
    tool = android2grd.Android2Grd()

    tool.ParseOptions(options)

    android_path = util.PathFromRoot('grit/testdata/android.xml')
    with open(android_path) as android_file:
      android_dom = xml.dom.minidom.parse(android_file)

    grd = tool.AndroidDomToGrdDom(android_dom)
    self.assertTrue(isinstance(grd, misc.GritNode))

    return grd

  def testAndroidDomToGrdDom(self):
    grd = self.__ParseAndroidXml(['--languages', 'en-US,en-GB,ru'])

    # Check that the structure of the GritNode is as expected.
    messages = grd.GetChildrenOfType(message.MessageNode)
    translations = grd.GetChildrenOfType(empty.TranslationsNode)
    files = grd.GetChildrenOfType(node_io.FileNode)

    self.assertEqual(len(translations), 1)
    self.assertEqual(len(files), 3)
    self.assertEqual(len(messages), 5)

    # Check that a message node is constructed correctly.
    msg = [x for x in messages if x.GetTextualIds()[0] == 'IDS_PLACEHOLDERS']
    self.assertTrue(msg)
    msg = msg[0]

    self.assertTrue(msg.IsTranslateable())
    self.assertEqual(msg.attrs["desc"], "A string with placeholder.")

  def testTranslatableAttribute(self):
    grd = self.__ParseAndroidXml([])
    messages = grd.GetChildrenOfType(message.MessageNode)
    msgs = [x for x in messages if x.GetTextualIds()[0] == 'IDS_CONSTANT']
    self.assertTrue(msgs)
    self.assertFalse(msgs[0].IsTranslateable())

  def testTranslations(self):
    grd = self.__ParseAndroidXml(['--languages', 'en-US,en-GB,ru,id'])

    files = grd.GetChildrenOfType(node_io.FileNode)
    us_file = [x for x in files if x.attrs['lang'] == 'en-US']
    self.assertTrue(us_file)
    self.assertEqual(us_file[0].GetInputPath(),
                     'chrome_android_strings_en-US.xtb')

    id_file = [x for x in files if x.attrs['lang'] == 'id']
    self.assertTrue(id_file)
    self.assertEqual(id_file[0].GetInputPath(),
                     'chrome_android_strings_id.xtb')

  def testOutputs(self):
    grd = self.__ParseAndroidXml(['--languages', 'en-US,ru,id',
                                  '--rc-dir', 'rc/dir',
                                  '--header-dir', 'header/dir',
                                  '--xtb-dir', 'xtb/dir',
                                  '--xml-dir', 'xml/dir'])

    outputs = grd.GetChildrenOfType(node_io.OutputNode)
    self.assertEqual(len(outputs), 7)

    header_outputs = [x for x in outputs if x.GetType() == 'rc_header']
    rc_outputs = [x for x in outputs if x.GetType() == 'rc_all']
    xml_outputs = [x for x in outputs if x.GetType() == 'android']

    self.assertEqual(len(header_outputs), 1)
    self.assertEqual(len(rc_outputs), 3)
    self.assertEqual(len(xml_outputs), 3)

    # The header node should have an "<emit>" child and the proper filename.
    self.assertTrue(header_outputs[0].GetChildrenOfType(node_io.EmitNode))
    self.assertEqual(util.normpath(header_outputs[0].GetFilename()),
                     util.normpath('header/dir/chrome_android_strings.h'))

    id_rc = [x for x in rc_outputs if x.GetLanguage() == 'id']
    id_xml = [x for x in xml_outputs if x.GetLanguage() == 'id']
    self.assertTrue(id_rc)
    self.assertTrue(id_xml)
    self.assertEqual(util.normpath(id_rc[0].GetFilename()),
                     util.normpath('rc/dir/chrome_android_strings_id.rc'))
    self.assertEqual(util.normpath(id_xml[0].GetFilename()),
                     util.normpath('xml/dir/values-in/strings.xml'))

    us_rc = [x for x in rc_outputs if x.GetLanguage() == 'en-US']
    us_xml = [x for x in xml_outputs if x.GetLanguage() == 'en-US']
    self.assertTrue(us_rc)
    self.assertTrue(us_xml)
    self.assertEqual(util.normpath(us_rc[0].GetFilename()),
                     util.normpath('rc/dir/chrome_android_strings_en-US.rc'))
    self.assertEqual(util.normpath(us_xml[0].GetFilename()),
                     util.normpath('xml/dir/values-en-rUS/strings.xml'))


if __name__ == '__main__':
  unittest.main()
