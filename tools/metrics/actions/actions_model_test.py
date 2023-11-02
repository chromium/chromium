# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from parameterized import parameterized
import unittest
import xml.dom.minidom

import actions_model

PRETTY_XML = """
<actions>

<action name="Action1">
  <owner>owner@chromium.org</owner>
  <description>Description1</description>
</action>

<action name="Action2" not_user_triggered="true">
  <obsolete>Obsolete text</obsolete>
  <owner>owner@chromium.org</owner>
  <description>Description2</description>
</action>

<action name="Action3">
  <obsolete>Obsolete text</obsolete>
  <owner>owner@chromium.org</owner>
  <owner>owner2@chromium.org</owner>
  <description>Description3</description>
</action>

<action-suffix separator="." ordering="suffix">
  <suffix name="AppMenu" label="Users opened a new incognito tab by app menu"/>
  <suffix name="LongTapMenu"
      label="Users opened a new incognito tab by long tap menu"/>
</action-suffix>

<action-suffix separator="." ordering="suffix">
  <suffix name="LongTapMenu" label="Users closed a tab by long tap menu"/>
  <affected-action name="AnAction"/>
  <affected-action name="MobileMenuCloseTab"/>
</action-suffix>

</actions>
""".strip()

XML_WITH_WRONG_INDENT = """
<actions>

  <action name="Action1">
    <owner>owner@chromium.org</owner>
    <description>Description1</description>
  </action>

<action name="Action2" not_user_triggered="true">
    <obsolete>Obsolete text</obsolete>
    <owner>owner@chromium.org</owner>
    <description>Description2</description>
</action>

<action name="Action3">
<obsolete>Obsolete text</obsolete>
<owner>owner@chromium.org</owner>
<owner>owner2@chromium.org</owner>
<description>Description3</description>
</action>

  <action-suffix separator="." ordering="suffix">
  <suffix name="AppMenu" label="Users opened a new incognito tab by app menu"/>
    <suffix name="LongTapMenu"
        label="Users opened a new incognito tab by long tap menu"/>
  </action-suffix>

  <action-suffix separator="." ordering="suffix">
    <suffix name="LongTapMenu" label="Users closed a tab by long tap menu"/>
    <affected-action name="AnAction"/>
    <affected-action name="MobileMenuCloseTab"/>
  </action-suffix>

</actions>
""".strip()

XML_WITH_WRONG_SINGLELINE = """
<actions>

<action name="Action1">
  <owner>
    owner@chromium.org
  </owner>
  <description>Description1</description>
</action>

<action name="Action2" not_user_triggered="true">
  <obsolete>
    Obsolete text
  </obsolete>
  <owner>owner@chromium.org</owner>
  <description>Description2</description>
</action>

<action name="Action3">
  <obsolete>Obsolete text</obsolete>
  <owner>owner@chromium.org</owner>
  <owner>owner2@chromium.org</owner>
  <description>
    Description3
  </description>
</action>

<action-suffix separator="." ordering="suffix">
  <suffix name="AppMenu" label="Users opened a new incognito tab by app menu"/>
  <suffix name="LongTapMenu"
      label="Users opened a new incognito tab by long tap menu"/>
</action-suffix>

<action-suffix separator="." ordering="suffix">
  <suffix name="LongTapMenu" label="Users closed a tab by long tap menu"/>
  <affected-action name="AnAction"/>
  <affected-action name="MobileMenuCloseTab"/>
</action-suffix>

</actions>
""".strip()

XML_WITH_WRONG_LINE_BREAK = """
<actions>
<action name="Action1">
  <owner>owner@chromium.org</owner>
  <description>Description1</description>
</action>

<action name="Action2" not_user_triggered="true">
  <obsolete>Obsolete text</obsolete>
  <owner>owner@chromium.org</owner>
  <description>Description2</description>
</action>
<action name="Action3">
  <obsolete>Obsolete text</obsolete>
  <owner>owner@chromium.org</owner>

  <owner>owner2@chromium.org</owner>
  <description>Description3</description>
</action>

<action-suffix separator="." ordering="suffix">
  <suffix name="AppMenu" label="Users opened a new incognito tab by app menu"/>
  <suffix name="LongTapMenu"
      label="Users opened a new incognito tab by long tap menu"/>
</action-suffix>

<action-suffix separator="." ordering="suffix">
  <suffix name="LongTapMenu"
      label="Users closed a tab by long tap menu"/>
  <affected-action name="AnAction"/>
  <affected-action name="MobileMenuCloseTab"/>
</action-suffix>
</actions>
""".strip()

XML_WITH_WRONG_ORDER = """
<actions>

<action name="Action2" not_user_triggered="true">
  <obsolete>Obsolete text</obsolete>
  <owner>owner@chromium.org</owner>
  <description>Description2</description>
</action>

<action name="Action1">
  <owner>owner@chromium.org</owner>
  <description>Description1</description>
</action>

<action name="Action3">
  <obsolete>Obsolete text</obsolete>
  <owner>owner@chromium.org</owner>
  <owner>owner2@chromium.org</owner>
  <description>Description3</description>
</action>

<action-suffix separator="." ordering="suffix">
  <suffix name="LongTapMenu"
      label="Users opened a new incognito tab by long tap menu"/>
  <suffix name="AppMenu" label="Users opened a new incognito tab by app menu"/>
</action-suffix>

<action-suffix separator="." ordering="suffix">
  <suffix name="LongTapMenu" label="Users closed a tab by long tap menu"/>
  <affected-action name="MobileMenuCloseTab"/>
  <affected-action name="AnAction"/>
</action-suffix>

</actions>
""".strip()

XML_WITH_WRONG_CHILDREN_ORDER = """
<actions>

<action name="Action1">
  <description>Description1</description>
  <owner>owner@chromium.org</owner>
</action>

<action name="Action2" not_user_triggered="true">
  <owner>owner@chromium.org</owner>
  <obsolete>Obsolete text</obsolete>
  <description>Description2</description>
</action>

<action name="Action3">
  <obsolete>Obsolete text</obsolete>
  <owner>owner@chromium.org</owner>
  <description>Description3</description>
  <owner>owner2@chromium.org</owner>
</action>

<action-suffix separator="." ordering="suffix">
  <suffix name="AppMenu" label="Users opened a new incognito tab by app menu"/>
  <suffix name="LongTapMenu"
      label="Users opened a new incognito tab by long tap menu"/>
</action-suffix>

<action-suffix separator="." ordering="suffix">
  <affected-action name="AnAction"/>
  <affected-action name="MobileMenuCloseTab"/>
  <suffix name="LongTapMenu" label="Users closed a tab by long tap menu"/>
</action-suffix>

</actions>
""".strip()

XML_WITH_WRONG_ATTRIBUTE_ORDER = """
<actions>

<action name="Action1">
  <owner>owner@chromium.org</owner>
  <description>Description1</description>
</action>

<action not_user_triggered="true" name="Action2">
  <obsolete>Obsolete text</obsolete>
  <owner>owner@chromium.org</owner>
  <description>Description2</description>
</action>

<action name="Action3">
  <obsolete>Obsolete text</obsolete>
  <owner>owner@chromium.org</owner>
  <owner>owner2@chromium.org</owner>
  <description>Description3</description>
</action>

<action-suffix ordering="suffix" separator=".">
  <suffix name="AppMenu" label="Users opened a new incognito tab by app menu"/>
  <suffix name="LongTapMenu"
      label="Users opened a new incognito tab by long tap menu"/>
</action-suffix>

<action-suffix separator="." ordering="suffix">
  <suffix label="Users closed a tab by long tap menu" name="LongTapMenu"/>
  <affected-action name="AnAction"/>
  <affected-action name="MobileMenuCloseTab"/>
</action-suffix>

</actions>
""".strip()


class ActionXmlTest(unittest.TestCase):
  def __init__(self, *args, **kwargs):
    super(ActionXmlTest, self).__init__(*args, **kwargs)
    self.maxDiff = None

  @parameterized.expand([
      # Test prettify already pretty XML to verify the pretty-printed version
      # is the same.
      ('AlreadyPrettyXml', PRETTY_XML, PRETTY_XML),
      ('Indent', XML_WITH_WRONG_INDENT, PRETTY_XML),
      ('SingleLine', XML_WITH_WRONG_SINGLELINE, PRETTY_XML),
      ('LineBreak', XML_WITH_WRONG_LINE_BREAK, PRETTY_XML),
      ('Order', XML_WITH_WRONG_ORDER, PRETTY_XML),
      # The children of <action> should be sorted in the order of <obsolete>,
      # <owner> and <description>
      ('ChildrenOrder', XML_WITH_WRONG_CHILDREN_ORDER, PRETTY_XML),
  ])
  def testPrettify(self, _, input_xml, expected_xml):
    result = actions_model.PrettifyTree(xml.dom.minidom.parseString(input_xml))
    self.assertMultiLineEqual(result.strip(), expected_xml)

  @parameterized.expand([
      ('BadAttributeBoolean', PRETTY_XML, 'true', 'hello', 'hello'),
      ('BadSuffixNameWithSpace', PRETTY_XML, 'AppMenu', 'App Menu', 'App Menu'),
      ('BadAffectedActionNameWithSpace', PRETTY_XML, 'AnAction', 'An Action',
       'An Action'),
      ('SuffixWithBadSeparator', PRETTY_XML, '.', '-', '-'),
      ('BadOrdering_IllegalWord', PRETTY_XML, 'ordering="suffix"',
       'ordering="hello"', 'hello'),
  ])
  def testRegex(self, _, pretty_input_xml, original_string, bad_string,
                error_string):
    BAD_XML = pretty_input_xml.replace(original_string, bad_string)
    with self.assertRaises(ValueError) as context:
      actions_model.PrettifyTree(xml.dom.minidom.parseString(BAD_XML))
    self.assertIn(error_string, str(context.exception))
    self.assertIn('does not match regex', str(context.exception))


if __name__ == '__main__':
  unittest.main()
