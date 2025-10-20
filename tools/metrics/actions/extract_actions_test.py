#!/usr/bin/env python3
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import action_utils
import extract_actions

# Empty value to be inserted to |ACTIONS_MOCK|.
NO_VALUE = ''

ONE_OWNER = '<owner>name1@google.com</owner>\n'
TWO_OWNERS = """
<owner>name1@google.com</owner>\n
<owner>name2@google.com</owner>\n
"""

DESCRIPTION = '<description>Description.</description>\n'
TWO_DESCRIPTIONS = """
<description>Description.</description>\n
<description>Description2.</description>\n
"""

OBSOLETE = '<obsolete>Not used anymore. Replaced by action2.</obsolete>\n'
TWO_OBSOLETE = '<obsolete>Obsolete1.</obsolete><obsolete>Obsolete2.</obsolete>'

COMMENT = '<!--comment-->'

NOT_USER_TRIGGERED = 'not_user_triggered="true"'

# A format string to mock the input action.xml file.
ACTIONS_XML = """
{comment}
<actions>

<action name="action1" {not_user_triggered}>
{obsolete}{owners}{description}
</action>

</actions>"""

NO_OWNER_EXPECTED_XML = ('<actions>\n\n'
                         '<action name="action1">\n'
                         '  <owner>Please list the metric\'s owners. '
                         'Add more owner tags as needed.</owner>\n'
                         '  <description>Description.</description>\n'
                         '</action>\n\n'
                         '</actions>\n')

ONE_OWNER_EXPECTED_XML = ('<actions>\n\n'
                          '<action name="action1">\n'
                          '  <owner>name1@google.com</owner>\n'
                          '  <description>Description.</description>\n'
                          '</action>\n\n'
                          '</actions>\n')

TWO_OWNERS_EXPECTED_XML = ('<actions>\n\n'
                           '<action name="action1">\n'
                           '  <owner>name1@google.com</owner>\n'
                           '  <owner>name2@google.com</owner>\n'
                           '  <description>Description.</description>\n'
                           '</action>\n\n'
                           '</actions>\n')

NO_DESCRIPTION_EXPECTED_XML = (
    '<actions>\n\n'
    '<action name="action1">\n'
    '  <owner>name1@google.com</owner>\n'
    '  <owner>name2@google.com</owner>\n'
    '  <description>Please enter the description of the metric.</description>\n'
    '</action>\n\n'
    '</actions>\n')

OBSOLETE_EXPECTED_XML = (
    '<actions>\n\n'
    '<action name="action1">\n'
    '  <obsolete>Not used anymore. Replaced by action2.</obsolete>\n'
    '  <owner>name1@google.com</owner>\n'
    '  <owner>name2@google.com</owner>\n'
    '  <description>Description.</description>\n'
    '</action>\n\n'
    '</actions>\n')

ADD_ACTION_EXPECTED_XML = (
    '<actions>\n\n'
    '<action name="action1">\n'
    '  <owner>name1@google.com</owner>\n'
    '  <owner>name2@google.com</owner>\n'
    '  <description>Description.</description>\n'
    '</action>\n\n'
    '<action name="action2">\n'
    '  <owner>Please list the metric\'s owners.'
    ' Add more owner tags as needed.</owner>\n'
    '  <description>Please enter the description of the metric.</description>\n'
    '</action>\n\n'
    '</actions>\n')

COMMENT_EXPECTED_XML = ('<!--comment-->\n\n'
                        '<actions>\n\n'
                        '<action name="action1">\n'
                        '  <owner>name1@google.com</owner>\n'
                        '  <owner>name2@google.com</owner>\n'
                        '  <description>Description.</description>\n'
                        '</action>\n\n'
                        '</actions>\n')

NOT_USER_TRIGGERED_EXPECTED_XML = (
    '<actions>\n\n'
    '<action name="action1" not_user_triggered="true">\n'
    '  <owner>Please list the metric\'s owners. '
    'Add more owner tags as needed.</owner>\n'
    '  <description>Description.</description>\n'
    '</action>\n\n'
    '</actions>\n')


BASIC_VARIANT_EXPANDED_XML = (
    '<actions>\n\n'
    '<action name="action1_variant1">\n'
    '  <owner>name1@chromium.org</owner>\n'
    '  <description>Description for {TestToken}.'
    ' Variant Description 1.</description>\n'
    '</action>\n\n'
    '<action name="action1{TestToken}">\n'
    '  <owner>name1@chromium.org</owner>\n'
    '  <description>Description for {TestToken}.</description>\n'
    '  <token key="TestToken">\n'
    '    <variant name="_variant1" summary="Variant Description 1."/>\n'
    '  </token>\n'
    '</action>\n\n'
    '</actions>\n')

MULTI_ACTION_MULTI_VARIANT_XML = (
    '<actions>\n\n'
    '<variants name="Variants1">\n'
    '  <variant name=".variant1" summary="Variant Description 1."/>\n'
    '  <variant name=".variant2" summary="Variant Description 2."/>\n'
    '</variants>\n\n'
    '<variants name="Variants2">\n'
    '  <variant name=".variant3" summary="Variant Description 3."/>\n'
    '</variants>\n\n'
    '<action name="action1.variant1">\n'
    '  <owner>name1@chromium.org</owner>\n'
    '  <description>Description1. Variant Description 1.</description>\n'
    '</action>\n\n'
    '<action name="action1.variant2">\n'
    '  <owner>name1@chromium.org</owner>\n'
    '  <description>Description1. Variant Description 2.</description>\n'
    '</action>\n\n'
    '<action name="action1{Token1}">\n'
    '  <owner>name1@chromium.org</owner>\n'
    '  <description>Description1.</description>\n'
    '  <token key="Token1" variants="Variants1"/>\n'
    '</action>\n\n'
    '<action name="action2.variant1">\n'
    '  <owner>name2@chromium.org</owner>\n'
    '  <description>Description2. Variant Description 1.</description>\n'
    '</action>\n\n'
    '<action name="action2.variant2">\n'
    '  <owner>name2@chromium.org</owner>\n'
    '  <description>Description2. Variant Description 2.</description>\n'
    '</action>\n\n'
    '<action name="action2{Token2}">\n'
    '  <owner>name2@chromium.org</owner>\n'
    '  <description>Description2.</description>\n'
    '  <token key="Token2" variants="Variants1"/>\n'
    '</action>\n\n'
    '<action name="action3.variant3">\n'
    '  <owner>name3@chromium.org</owner>\n'
    '  <description>Description3. Variant Description 3.</description>\n'
    '</action>\n\n'
    '<action name="action3{Token3}">\n'
    '  <owner>name3@chromium.org</owner>\n'
    '  <description>Description3.</description>\n'
    '  <token key="Token3" variants="Variants2"/>\n'
    '</action>\n\n'
    '</actions>\n')


class ActionXmlTest(unittest.TestCase):

  def _GetProcessedAction(self,
                          owner,
                          description,
                          obsolete,
                          not_user_triggered=NO_VALUE,
                          new_actions=[],
                          comment=NO_VALUE):
    """Forms an actions XML string and returns it after processing.

    It parses the original XML string, adds new user actions (if there is any),
    and pretty prints it.

    Args:
      owner: the owner tag to be inserted in the original XML string.
      description: the description tag to be inserted in the original XML
        string.
      obsolete: the obsolete tag to be inserted in the original XML string.
      new_actions: optional. List of new user actions' names to be inserted.
      comment: the comment tag to be inserted in the original XML string.

    Returns:
      An updated and pretty-printed action XML string.
    """
    # Form the actions.xml mock content based on the input parameters.
    current_xml = ACTIONS_XML.format(owners=owner,
                                     description=description,
                                     obsolete=obsolete,
                                     comment=comment,
                                     not_user_triggered=not_user_triggered)
    actions_dict, comments, variants_dict = extract_actions.ParseActionFile(
        current_xml)
    for action_name in new_actions:
      actions_dict[action_name] = action_utils.Action(action_name, None, [])
    return extract_actions.PrettyPrint(actions_dict, comments, variants_dict)

  def _ExpandVariantsInActionsXML(self, actions_xml):
    """Parses the given actions XML, expands variants and pretty prints it.

    Args:
      actions_xml: actions XML string.

    Returns:
      An updated and pretty-printed actions XML string with variants expanded.
    """
    actions_dict, comments, variants_dict = extract_actions.ParseActionFile(
        actions_xml)
    action_utils.CreateActionsFromVariants(actions_dict, variants_dict)
    return extract_actions.PrettyPrint(actions_dict, comments, variants_dict)


  def _PrettyPrintActionsXML(self, actions_xml):
    """Parses the given actions XML and pretty prints it.

    Args:
      actions_xml: actions XML string.

    Returns:
      A pretty-printed actions XML string.
    """
    actions_dict, comments, variants_dict = extract_actions.ParseActionFile(
        actions_xml)
    return extract_actions.PrettyPrint(actions_dict, comments, variants_dict)

  def testNoOwner(self):
    xml_result = self._GetProcessedAction(NO_VALUE, DESCRIPTION, NO_VALUE)
    self.assertEqual(NO_OWNER_EXPECTED_XML, xml_result)

  def testOneOwnerOneDescription(self):
    xml_result = self._GetProcessedAction(ONE_OWNER, DESCRIPTION, NO_VALUE)
    self.assertEqual(ONE_OWNER_EXPECTED_XML, xml_result)

  def testTwoOwners(self):
    xml_result = self._GetProcessedAction(TWO_OWNERS, DESCRIPTION, NO_VALUE)
    self.assertEqual(TWO_OWNERS_EXPECTED_XML, xml_result)

  def testNoDescription(self):
    xml_result = self._GetProcessedAction(TWO_OWNERS, NO_VALUE, NO_VALUE)
    self.assertEqual(NO_DESCRIPTION_EXPECTED_XML, xml_result)

  def testTwoDescriptions(self):
    current_xml = ACTIONS_XML.format(owners=TWO_OWNERS,
                                     obsolete=NO_VALUE,
                                     description=TWO_DESCRIPTIONS,
                                     comment=NO_VALUE,
                                     not_user_triggered=NO_VALUE)
    # Since there are two description tags, the function ParseActionFile will
    # raise SystemExit with exit code 1.
    with self.assertRaises(SystemExit) as cm:
      extract_actions.ParseActionFile(current_xml)
    self.assertEqual(cm.exception.code, 1)

  def testObsolete(self):
    xml_result = self._GetProcessedAction(TWO_OWNERS, DESCRIPTION, OBSOLETE)
    self.assertEqual(OBSOLETE_EXPECTED_XML, xml_result)

  def testTwoObsoletes(self):
    current_xml = ACTIONS_XML.format(owners=TWO_OWNERS,
                                     obsolete=TWO_OBSOLETE,
                                     description=DESCRIPTION,
                                     comment=NO_VALUE,
                                     not_user_triggered=NO_VALUE)

    # Since there are two obsolete tags, the function ParseActionFile will
    # raise SystemExit with exit code 1.
    with self.assertRaises(SystemExit) as cm:
      extract_actions.ParseActionFile(current_xml)
    self.assertEqual(cm.exception.code, 1)

  def testAddNewActions(self):
    xml_result = self._GetProcessedAction(TWO_OWNERS,
                                          DESCRIPTION,
                                          NO_VALUE,
                                          new_actions=['action2'])
    self.assertEqual(ADD_ACTION_EXPECTED_XML, xml_result)

  def testComment(self):
    xml_result = self._GetProcessedAction(TWO_OWNERS,
                                          DESCRIPTION,
                                          NO_VALUE,
                                          comment=COMMENT)
    self.assertEqual(COMMENT_EXPECTED_XML, xml_result)

  def testNotUserTriggered(self):
    xml_result = self._GetProcessedAction(NO_VALUE, DESCRIPTION, NO_VALUE,
                                          NOT_USER_TRIGGERED)
    self.assertEqual(NOT_USER_TRIGGERED_EXPECTED_XML, xml_result)

  def testUserMetricsActionSpanningTwoLines(self):
    code = 'base::UserMetricsAction(\n"Foo.Bar"));'
    finder = extract_actions.ActionNameFinder(
        'dummy', code, extract_actions.USER_METRICS_ACTION_RE)
    self.assertEqual('Foo.Bar', finder.FindNextAction())
    self.assertFalse(finder.FindNextAction())

  def testUserMetricsActionAsAParam(self):
    code = 'base::UserMetricsAction("Test.Foo"), "Test.Bar");'
    finder = extract_actions.ActionNameFinder(
        'dummy', code, extract_actions.USER_METRICS_ACTION_RE)
    self.assertEqual('Test.Foo', finder.FindNextAction())
    self.assertFalse(finder.FindNextAction())

  def testNonLiteralUserMetricsAction(self):
    code = 'base::UserMetricsAction(FOO)'
    finder = extract_actions.ActionNameFinder(
        'dummy', code, extract_actions.USER_METRICS_ACTION_RE)
    self.assertIsNone(finder.FindNextAction())

  def testTernaryUserMetricsAction(self):
    code = 'base::UserMetricsAction(foo ? "Foo.Bar" : "Bar.Foo"));'
    finder = extract_actions.ActionNameFinder(
        'dummy', code, extract_actions.USER_METRICS_ACTION_RE)
    self.assertIsNone(finder.FindNextAction())

  def testTernaryUserMetricsActionWithNewLines(self):
    code = """base::UserMetricsAction(
      foo_bar ? "Bar.Foo" :
      "Foo.Car")"""
    finder = extract_actions.ActionNameFinder(
        'dummy', code, extract_actions.USER_METRICS_ACTION_RE)
    self.assertIsNone(finder.FindNextAction())

  def testUserMetricsActionWithExtraWhitespace(self):
    code = """base::UserMetricsAction("Foo.Bar" )"""
    finder = extract_actions.ActionNameFinder(
        'dummy', code, extract_actions.USER_METRICS_ACTION_RE)
    self.assertEqual('Foo.Bar', finder.FindNextAction())

  def testUserMetricsActionWithStringConcatenation(self):
    code = 'base::UserMetricsAction("Foo.Bar" "Baz.Qux")'
    finder = extract_actions.ActionNameFinder(
        'dummy', code, extract_actions.USER_METRICS_ACTION_RE)
    self.assertEqual('Foo.BarBaz.Qux', finder.FindNextAction())

  def testUserMetricsActionWithStringConcatenationWithPlus(self):
    code = 'base::UserMetricsAction("Foo.Bar" + "Baz.Qux")'
    finder = extract_actions.ActionNameFinder(
        'dummy', code, extract_actions.USER_METRICS_ACTION_RE)
    self.assertIsNone(finder.FindNextAction())

  def testUserMetricsActionWithEscapedQuotes(self):
    code = 'base::UserMetricsAction("Foo.Bar\\"Baz")'
    finder = extract_actions.ActionNameFinder(
        'dummy', code, extract_actions.USER_METRICS_ACTION_RE)
    self.assertEqual('Foo.Bar"Baz', finder.FindNextAction())

  def testUserMetricsActionWithMixedQuotes(self):
    code = """base::UserMetricsAction('Foo."Bar"' )"""
    finder = extract_actions.ActionNameFinder(
        'dummy', code, extract_actions.USER_METRICS_ACTION_RE)
    self.assertEqual('Foo."Bar"', finder.FindNextAction())

  def testUserMetricsActionSpanningTwoLinesJs(self):
    code = "chrome.send('coreOptionsUserMetricsAction',\n['Foo.Bar']);"
    finder = extract_actions.ActionNameFinder(
        'dummy', code, extract_actions.USER_METRICS_ACTION_RE_JS)
    self.assertEqual('Foo.Bar', finder.FindNextAction())
    self.assertIsNone(finder.FindNextAction())

  def testNonLiteralUserMetricsActionJs(self):
    code = "chrome.send('coreOptionsUserMetricsAction',\n[FOO]);"
    finder = extract_actions.ActionNameFinder(
        'dummy', code, extract_actions.USER_METRICS_ACTION_RE_JS)
    self.assertIsNone(finder.FindNextAction())

  def testTernaryUserMetricsActionJs(self):
    code = ("chrome.send('coreOptionsUserMetricsAction', "
            "[foo ? 'Foo.Bar' : 'Bar.Foo']);")
    finder = extract_actions.ActionNameFinder(
        'dummy', code, extract_actions.USER_METRICS_ACTION_RE_JS)
    self.assertIsNone(finder.FindNextAction())

  def testTernaryUserMetricsActionWithNewLinesJs(self):
    code = """chrome.send('coreOptionsUserMetricsAction',
      [foo ? 'Foo.Bar' :
      'Bar.Foo']);"""
    finder = extract_actions.ActionNameFinder(
        'dummy', code, extract_actions.USER_METRICS_ACTION_RE_JS)
    self.assertIsNone(finder.FindNextAction())

  def testUserMetricsActionWithExtraCharactersJs(self):
    code = """chrome.send('coreOptionsUserMetricsAction',
      ['Foo.Bar' + 1]);"""
    finder = extract_actions.ActionNameFinder(
        'dummy', code, extract_actions.USER_METRICS_ACTION_RE_JS)
    self.assertIsNone(finder.FindNextAction())

  def testComputedUserMetricsActionJs(self):
    code = """chrome.send('coreOptionsUserMetricsAction',
      ['Foo.' + foo_bar ? 'Bar' : 'Foo']);"""
    finder = extract_actions.ActionNameFinder(
        'dummy', code, extract_actions.USER_METRICS_ACTION_RE_JS)
    self.assertIsNone(finder.FindNextAction())

  def testUserMetricsActionWithMismatchedQuotes(self):
    code = "chrome.send('coreOptionsUserMetricsAction', [\"Foo.Bar']);"
    finder = extract_actions.ActionNameFinder(
        'dummy', code, extract_actions.USER_METRICS_ACTION_RE_JS)
    self.assertIsNone(finder.FindNextAction())

  def testUserMetricsActionFromPropertyJs(self):
    code = "chrome.send('coreOptionsUserMetricsAction', [objOrArray[key]]);"
    finder = extract_actions.ActionNameFinder(
        'dummy', code, extract_actions.USER_METRICS_ACTION_RE_JS)
    self.assertIsNone(finder.FindNextAction())

  def testUserMetricsActionFromFunctionJs(self):
    code = "chrome.send('coreOptionsUserMetricsAction', [getAction(param)]);"
    finder = extract_actions.ActionNameFinder(
        'dummy', code, extract_actions.USER_METRICS_ACTION_RE_JS)
    self.assertIsNone(finder.FindNextAction())

  def testBasicVariant(self):
    original_xml = """
    <actions>
    <action name="action1{TestToken}">
      <owner>name1@chromium.org</owner>
      <description>Description for {TestToken}.</description>
      <token key="TestToken">
        <variant name="_variant1" summary="Variant Description 1."/>
      </token>
    </action>
    </actions>
    """
    xml_result = self._ExpandVariantsInActionsXML(original_xml)
    self.assertMultiLineEqual(BASIC_VARIANT_EXPANDED_XML, xml_result)

  def testCreateActionFromVariantWithTokenInMiddle(self):
    actions_dict = {}
    action = action_utils.Action('TestAction{Token}Name', 'Test description.',
                                 ['owner@chromium.org'])
    variant = action_utils.Variant('Variant', 'Variant summary.')
    token = action_utils.Token('Token')

    action_utils._CreateActionFromVariant(actions_dict, action, variant, token)

    self.assertIn('TestActionVariantName', actions_dict)
    new_action = actions_dict['TestActionVariantName']
    self.assertEqual('TestActionVariantName', new_action.name)
    self.assertEqual('Test description. Variant summary.',
                     new_action.description)

  def testVariantPrettyPrint(self):
    """Tests that variants are preserved when pretty-printing."""
    original_xml = """<actions>
  <variants name="TestVariants">
    <variant name="_variant1" summary="Variant Description 1."/>
  </variants>
  <action name="action1{TestToken}">
    <owner>name1@chromium.org</owner>
    <description>Description.</description>
    <token key="TestToken" variants="TestVariants"/>
  </action>
  </actions>
  """
    xml_result = self._PrettyPrintActionsXML(original_xml)
    expected_pretty_xml = """<actions>

<variants name="TestVariants">
  <variant name="_variant1" summary="Variant Description 1."/>
</variants>

<action name="action1{TestToken}">
  <owner>name1@chromium.org</owner>
  <description>Description.</description>
  <token key="TestToken" variants="TestVariants"/>
</action>

</actions>
"""
    self.assertMultiLineEqual(expected_pretty_xml, xml_result)

  def testMultiActionMultiVariant(self):
    """Tests multiple actions using multiple variants blocks."""
    original_xml = """
    <actions>
      <variants name="Variants1">
        <variant name=".variant1" summary="Variant Description 1."/>
        <variant name=".variant2" summary="Variant Description 2."/>
      </variants>
      <variants name="Variants2">
        <variant name=".variant3" summary="Variant Description 3."/>
      </variants>
      <action name="action1{Token1}">
        <owner>name1@chromium.org</owner>
        <description>Description1.</description>
        <token key="Token1" variants="Variants1"/>
      </action>
      <action name="action2{Token2}">
        <owner>name2@chromium.org</owner>
        <description>Description2.</description>
        <token key="Token2" variants="Variants1"/>
      </action>
      <action name="action3{Token3}">
        <owner>name3@chromium.org</owner>
        <description>Description3.</description>
        <token key="Token3" variants="Variants2"/>
      </action>
    </actions>
    """
    xml_result = self._ExpandVariantsInActionsXML(original_xml)
    self.assertMultiLineEqual(MULTI_ACTION_MULTI_VARIANT_XML, xml_result)


if __name__ == '__main__':
  unittest.main()
