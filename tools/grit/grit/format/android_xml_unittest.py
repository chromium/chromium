#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittest for android_xml.py."""


import io
import os
import sys
import unittest

if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

from io import StringIO

from grit import constants
from grit import grd_reader
from grit import util
from grit.format import android_xml
from grit.node import message
from grit.tool import build


class AndroidXmlUnittest(unittest.TestCase):

  def testMessages(self):
    root = util.ParseGrdForUnittest(r"""
        <messages>
          <message name="IDS_SIMPLE" desc="A vanilla string">
            Martha
          </message>
          <message name="IDS_ONE_LINE" desc="On one line">sat and wondered</message>
          <message name="IDS_QUOTES" desc="A string with quotation marks">
            out loud, "Why don't I build a flying car?"
          </message>
          <message name="IDS_MULTILINE" desc="A string split over several lines">
            She gathered
wood, charcoal, and
a sledge hammer.
          </message>
          <message name="IDS_WHITESPACE" desc="A string with extra whitespace.">
            '''   How old fashioned  --  she thought. '''
          </message>
          <message name="IDS_PLACEHOLDERS" desc="A string with placeholders">
            I'll buy a <ph name="WAVELENGTH">%d<ex>200</ex></ph> nm laser at <ph name="STORE_NAME">%s<ex>the grocery store</ex></ph>.
          </message>
          <message name="IDS_PLURALS" desc="A string using the ICU plural format">
            {NUM_THINGS, plural,
            =1 {Maybe I'll get one laser.}
            other {Maybe I'll get # lasers.}}
          </message>
          <message name="IDS_PLURALS_NO_SPACE" desc="A string using the ICU plural format with no space">
            {NUM_MISSISSIPPIS, plural,
            =1{OneMississippi}other{ManyMississippis}}
          </message>
          <message name="IDS_PLURALS_PSEUDO_LONG" desc="A string using the ICU plurals format after being transformed to en-XA">
            {MSG_COUNT, plural,
            =1 {Only one message}
            other {# messages}} - one two three four
          </message>
        </messages>
        """)

    buf = StringIO()
    build.RcBuilder.ProcessNode(
        root, DummyOutput('android', 'en', constants.DEFAULT_GENDER), buf)
    output = buf.getvalue()
    expected = r"""
<?xml version="1.0" encoding="utf-8"?>
<resources xmlns:android="http://schemas.android.com/apk/res/android">
<string name="simple">"Martha"</string>
<string name="one_line">"sat and wondered"</string>
<string name="quotes">"out loud, \"Why don\'t I build a flying car?\""</string>
<string name="multiline">"She gathered
wood, charcoal, and
a sledge hammer."</string>
<string name="whitespace">"   How old fashioned  --  she thought. "</string>
<string name="placeholders">"I\'ll buy a %d nm laser at %s."</string>
<plurals name="plurals">
  <item quantity="one">"Maybe I\'ll get one laser."</item>
  <item quantity="other">"Maybe I\'ll get %d lasers."</item>
</plurals>
<plurals name="plurals_no_space">
  <item quantity="one">"OneMississippi"</item>
  <item quantity="other">"ManyMississippis"</item>
</plurals>
<plurals name="plurals_pseudo_long">
  <item quantity="one">"Only one message - one two three four"</item>
  <item quantity="other">"%d messages - one two three four"</item>
</plurals>
</resources>
"""
    self.assertEqual(output.strip(), expected.strip())


  def testConflictingPlurals(self):
    root = util.ParseGrdForUnittest(r"""
        <messages>
          <message name="IDS_PLURALS" desc="A string using the ICU plural format">
            {NUM_THINGS, plural,
            =1 {Maybe I'll get one laser.}
            one {Maybe I'll get one laser.}
            other {Maybe I'll get # lasers.}}
          </message>
        </messages>
        """)

    buf = StringIO()
    build.RcBuilder.ProcessNode(
        root, DummyOutput('android', 'en', constants.DEFAULT_GENDER), buf)
    output = buf.getvalue()
    expected = r"""
<?xml version="1.0" encoding="utf-8"?>
<resources xmlns:android="http://schemas.android.com/apk/res/android">
<plurals name="plurals">
  <item quantity="one">"Maybe I\'ll get one laser."</item>
  <item quantity="other">"Maybe I\'ll get %d lasers."</item>
</plurals>
</resources>
"""
    self.assertEqual(output.strip(), expected.strip())


  def testTaggedOnly(self):
    root = util.ParseGrdForUnittest(r"""
        <messages>
          <message name="IDS_HELLO" desc="" formatter_data="android_java">
            Hello
          </message>
          <message name="IDS_WORLD" desc="">
            world
          </message>
        </messages>
        """)

    msg_hello, msg_world = root.GetChildrenOfType(message.MessageNode)
    self.assertTrue(android_xml.ShouldOutputNode(msg_hello, tagged_only=True))
    self.assertFalse(android_xml.ShouldOutputNode(msg_world, tagged_only=True))
    self.assertTrue(android_xml.ShouldOutputNode(msg_hello, tagged_only=False))
    self.assertTrue(android_xml.ShouldOutputNode(msg_world, tagged_only=False))


  def testFormat(self):
    xml = '''<?xml version="1.0" encoding="UTF-8"?>
      <grit latest_public_release="2" source_lang_id="en-US" current_release="3" base_dir=".">
        <translations>
          <file path="generated_resources_fr.xtb" lang="fr" />
        </translations>
        <outputs>
          <output filename="contents.pak" type="data_pack" />
        </outputs>
        <release seq="3">
          <messages>
            <message name="IDS_HELLO">Hello!</message>
            <message name="IDS_HELLO_USER">Hello <ph name="USERNAME">%s<ex>Joi</ex></ph></message>
            <message name="IDS_PLUGIN_NOT_AUTHORIZED">Plugin <ph name="PLUGIN_NAME">%s<ex>Pluggy McPluginface</ex></ph> is not authorized.</message>
            <message name="IDS_OPEN_IN_WINDOW">Open in a window</message>
          </messages>
        </release>
      </grit>'''
    grd = grd_reader.Parse(io.StringIO(xml),
                           util.PathFromRoot('grit/testdata'),
                           translate_genders=True)
    grd.SetOutputLanguage('en')
    grd.RunGatherers()
    grd.InitializeIds()

    expected_en_other = """<?xml version="1.0" encoding="utf-8"?>
<resources xmlns:android="http://schemas.android.com/apk/res/android">
<string name="hello">"Hello!"</string>
<string name="hello_user">"Hello %s"</string>
<string name="plugin_not_authorized">"Plugin %s is not authorized."</string>
<string name="open_in_window">"Open in a window"</string>
</resources>
"""

    # en has no gender translations, so the non-default-gendered xml data is
    # (effectively) empty.
    expected_en_neuter = """<?xml version="1.0" encoding="utf-8"?>
<resources xmlns:android="http://schemas.android.com/apk/res/android">
</resources>
"""
    expected_en_feminine = expected_en_neuter
    expected_en_masculine = expected_en_neuter

    # The "Open in a window" string gets deduped because it is the same in 'en'
    # and 'fr'.
    expected_fr_other = """<?xml version="1.0" encoding="utf-8"?>
<resources xmlns:android="http://schemas.android.com/apk/res/android">
<string name="hello">"Salut!"</string>
<string name="hello_user">"Salut %s"</string>
<string name="plugin_not_authorized">"Le plug-in %s n\\'est pas autorisé. (OTHER)"</string>
</resources>
"""

    # The only gendered translation in the xtb file for fr is included in the
    # xml data, but the genderless translations are deduped.
    expected_fr_neuter = """<?xml version="1.0" encoding="utf-8"?>
<resources xmlns:android="http://schemas.android.com/apk/res/android">
<string name="plugin_not_authorized">"Le plug-in %s n\\'est pas autorisé. (NEUTER)"</string>
</resources>
"""
    expected_fr_feminine = """<?xml version="1.0" encoding="utf-8"?>
<resources xmlns:android="http://schemas.android.com/apk/res/android">
<string name="plugin_not_authorized">"Le plug-in %s n\\'est pas autorisé. (FEMININE)"</string>
</resources>
"""
    expected_fr_masculine = """<?xml version="1.0" encoding="utf-8"?>
<resources xmlns:android="http://schemas.android.com/apk/res/android">
<string name="plugin_not_authorized">"Le plug-in %s n\\'est pas autorisé. (MASCULINE)"</string>
</resources>
"""

    def format(lang, gender):
      return ''.join(line for line in android_xml.Format(grd, lang, gender))

    self.assertEqual(format('en', constants.GENDER_OTHER), expected_en_other)
    self.assertEqual(format('en', constants.GENDER_NEUTER), expected_en_neuter)
    self.assertEqual(format('en', constants.GENDER_FEMININE),
                     expected_en_feminine)
    self.assertEqual(format('en', constants.GENDER_MASCULINE),
                     expected_en_masculine)
    self.assertEqual(format('fr', constants.GENDER_OTHER), expected_fr_other)
    self.assertEqual(format('fr', constants.GENDER_NEUTER), expected_fr_neuter)
    self.assertEqual(format('fr', constants.GENDER_FEMININE),
                     expected_fr_feminine)
    self.assertEqual(format('fr', constants.GENDER_MASCULINE),
                     expected_fr_masculine)


class DummyOutput:

  def __init__(self, type, language, gender):
    self.type = type
    self.language = language
    self.gender = gender

  def GetType(self):
    return self.type

  def GetLanguage(self):
    return self.language

  def GetOutputFilename(self):
    return 'hello.gif'

  def GetGender(self):
    return self.gender

if __name__ == '__main__':
  unittest.main()
