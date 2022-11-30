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

NO_OWNER_EXPECTED_XML = (
    '<actions>\n\n'
    '<action name="action1">\n'
    '  <owner>Please list the metric\'s owners. '
    'Add more owner tags as needed.</owner>\n'
    '  <description>Description.</description>\n'
    '</action>\n\n'
    '</actions>\n'
)

ONE_OWNER_EXPECTED_XML = (
    '<actions>\n\n'
    '<action name="action1">\n'
    '  <owner>name1@google.com</owner>\n'
    '  <description>Description.</description>\n'
    '</action>\n\n'
    '</actions>\n'
)

TWO_OWNERS_EXPECTED_XML = (
    '<actions>\n\n'
    '<action name="action1">\n'
    '  <owner>name1@google.com</owner>\n'
    '  <owner>name2@google.com</owner>\n'
    '  <description>Description.</description>\n'
    '</action>\n\n'
    '</actions>\n'
)

NO_DESCRIPTION_EXPECTED_XML = (
    '<actions>\n\n'
    '<action name="action1">\n'
    '  <owner>name1@google.com</owner>\n'
    '  <owner>name2@google.com</owner>\n'
    '  <description>Please enter the description of the metric.</description>\n'
    '</action>\n\n'
    '</actions>\n'
)

OBSOLETE_EXPECTED_XML = (
    '<actions>\n\n'
    '<action name="action1">\n'
    '  <obsolete>Not used anymore. Replaced by action2.</obsolete>\n'
    '  <owner>name1@google.com</owner>\n'
    '  <owner>name2@google.com</owner>\n'
    '  <description>Description.</description>\n'
    '</action>\n\n'
    '</actions>\n'
)

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
    '</actions>\n'
)

COMMENT_EXPECTED_XML = (
    '<!--comment-->\n\n'
    '<actions>\n\n'
    '<action name="action1">\n'
    '  <owner>name1@google.com</owner>\n'
    '  <owner>name2@google.com</owner>\n'
    '  <description>Description.</description>\n'
    '</action>\n\n'
    '</actions>\n'
)

NOT_USER_TRIGGERED_EXPECTED_XML = (
    '<actions>\n\n'
    '<action name="action1" not_user_triggered="true">\n'
    '  <owner>Please list the metric\'s owners. '
    'Add more owner tags as needed.</owner>\n'
    '  <description>Description.</description>\n'
    '</action>\n\n'
    '</actions>\n'
)

BASIC_SUFFIX_EXPECTED_XML = (
    '<actions>\n\n'
    '<action name="action1">\n'
    '  <owner>name1@chromium.org</owner>\n'
    '  <description>Description.</description>\n'
    '</action>\n\n'
    '<action name="action1_suffix1">\n'
    '  <owner>name1@chromium.org</owner>\n'
    '  <description>Description. Suffix Description 1.</description>\n'
    '</action>\n\n'
    '</actions>\n'
)

MULTI_ACTION_MULTI_SUFFIX_CHAIN = (
    '<actions>\n\n'
    '<action name="action1">\n'
    '  <owner>name1@chromium.org</owner>\n'
    '  <description>Description.</description>\n'
    '</action>\n\n'
    '<action name="action1_suffix1">\n'
    '  <owner>name1@chromium.org</owner>\n'
    '  <description>Description. Suffix Description 1.</description>\n'
    '</action>\n\n'
    '<action name="action1_suffix2">\n'
    '  <owner>name1@chromium.org</owner>\n'
    '  <description>Description. Suffix Description 2.</description>\n'
    '</action>\n\n'
    '<action name="action1_suffix2_suffix3">\n'
    '  <owner>name1@chromium.org</owner>\n'
    '  <description>\n'
    '    Description. Suffix Description 2. Suffix Description 3.\n'
    '  </description>\n'
    '</action>\n\n'
    '<action name="action1_suffix2_suffix3_suffix4">\n'
    '  <owner>name1@chromium.org</owner>\n'
    '  <description>\n'
    '    Description. Suffix Description 2. Suffix Description 3. '
    'Suffix Description\n'
    '    4.\n'
    '  </description>\n'
    '</action>\n\n'
    '<action name="action2">\n'
    '  <owner>name2@chromium.org</owner>\n'
    '  <description>Description.</description>\n'
    '</action>\n\n'
    '<action name="action2_suffix1">\n'
    '  <owner>name2@chromium.org</owner>\n'
    '  <description>Description. Suffix Description 1.</description>\n'
    '</action>\n\n'
    '<action name="action2_suffix2">\n'
    '  <owner>name2@chromium.org</owner>\n'
    '  <description>Description. Suffix Description 2.</description>\n'
    '</action>\n\n'
    '</actions>\n'
)

SUFFIX_CUSTOM_SEPARATOR = (
    '<actions>\n\n'
    '<action name="action1">\n'
    '  <owner>name1@chromium.org</owner>\n'
    '  <description>Description.</description>\n'
    '</action>\n\n'
    '<action name="action1.suffix1">\n'
    '  <owner>name1@chromium.org</owner>\n'
    '  <description>Description. Suffix Description 1.</description>\n'
    '</action>\n\n'
    '</actions>\n'
)

SUFFIX_OREDERING_PREFIX = (
    '<actions>\n\n'
    '<action name="action1.prefix1_remainder">\n'
    '  <owner>name1@chromium.org</owner>\n'
    '  <description>Description. Prefix Description 1.</description>\n'
    '</action>\n\n'
    '<action name="action1.remainder">\n'
    '  <owner>name1@chromium.org</owner>\n'
    '  <description>Description.</description>\n'
    '</action>\n\n'
    '</actions>\n'
)

AFFECTED_ACTION_WITH_SUFFIX_TAG = (
    '<actions>\n\n'
    '<action name="action1">\n'
    '  <owner>name1@chromium.org</owner>\n'
    '  <description>Description.</description>\n'
    '</action>\n\n'
    '<action name="action1_suffix1">\n'
    '  <owner>name1@chromium.org</owner>\n'
    '  <description>Description. Suffix Description 1.</description>\n'
    '</action>\n\n'
    '<action name="action1_suffix2">\n'
    '  <owner>name1@chromium.org</owner>\n'
    '  <description>Description. Suffix Description 2.</description>\n'
    '</action>\n\n'
    '<action name="action2">\n'
    '  <owner>name2@chromium.org</owner>\n'
    '  <description>Description.</description>\n'
    '</action>\n\n'
    '<action name="action2_suffix2">\n'
    '  <owner>name2@chromium.org</owner>\n'
    '  <description>Description. Suffix Description 2.</description>\n'
    '</action>\n\n'
    '</actions>\n'
)

class ActionXmlTest(unittest.TestCase):

  def _GetProcessedAction(self, owner, description, obsolete,
                          not_user_triggered=NO_VALUE, new_actions=[],
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
    current_xml = ACTIONS_XML.format(owners=owner, description=description,
                                     obsolete=obsolete, comment=comment,
                                     not_user_triggered=not_user_triggered)
    actions_dict, comments, suffixes = extract_actions.ParseActionFile(
        current_xml)
    for action_name in new_actions:
      actions_dict[action_name] = action_utils.Action(action_name, None, [])
    return extract_actions.PrettyPrint(actions_dict, comments, suffixes)

  def _ExpandSuffixesInActionsXML(self, actions_xml):
    """Parses the given actions XML, expands suffixes and pretty prints it.

    Args:
      actions_xml: actions XML string.

    Returns:
      An updated and pretty-printed actions XML string with suffixes expanded.
    """
    actions_dict, comments, suffixes = extract_actions.ParseActionFile(
        actions_xml)
    # Clear suffixes and mark actions as not coming from suffixes, so that
    # the returned XML file is the expanded one.
    suffixes = []
    for action in actions_dict.values():
      action.from_suffix = False
    return extract_actions.PrettyPrint(actions_dict, comments, suffixes)

  def _PrettyPrintActionsXML(self, actions_xml):
    """Parses the given actions XML and pretty prints it.

    Args:
      actions_xml: actions XML string.

    Returns:
      A pretty-printed actions XML string.
    """
    actions_dict, comments, suffixes = extract_actions.ParseActionFile(
        actions_xml)
    return extract_actions.PrettyPrint(actions_dict, comments, suffixes)

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
    current_xml = ACTIONS_XML.format(owners=TWO_OWNERS, obsolete=NO_VALUE,
                                     description=TWO_DESCRIPTIONS,
                                     comment=NO_VALUE,
                                     not_user_triggered=NO_VALUE)
    # Since there are two description tags, the function ParseActionFile will
    # raise SystemExit with exit code 1.
    with self.assertRaises(SystemExit) as cm:
      _, _ = extract_actions.ParseActionFile(current_xml)
    self.assertEqual(cm.exception.code, 1)

  def testObsolete(self):
    xml_result = self._GetProcessedAction(TWO_OWNERS, DESCRIPTION, OBSOLETE)
    self.assertEqual(OBSOLETE_EXPECTED_XML, xml_result)


  def testTwoObsoletes(self):
    current_xml = ACTIONS_XML.format(owners=TWO_OWNERS, obsolete=TWO_OBSOLETE,
                                     description=DESCRIPTION, comment=NO_VALUE,
                                     not_user_triggered=NO_VALUE)

    # Since there are two obsolete tags, the function ParseActionFile will
    # raise SystemExit with exit code 1.
    with self.assertRaises(SystemExit) as cm:
      _, _ = extract_actions.ParseActionFile(current_xml)
    self.assertEqual(cm.exception.code, 1)

  def testAddNewActions(self):
    xml_result = self._GetProcessedAction(TWO_OWNERS, DESCRIPTION, NO_VALUE,
                                          new_actions=['action2'])
    self.assertEqual(ADD_ACTION_EXPECTED_XML, xml_result)

  def testComment(self):
    xml_result = self._GetProcessedAction(TWO_OWNERS, DESCRIPTION, NO_VALUE,
                                          comment=COMMENT)
    self.assertEqual(COMMENT_EXPECTED_XML, xml_result)

  def testNotUserTriggered(self):
    xml_result = self._GetProcessedAction(NO_VALUE, DESCRIPTION, NO_VALUE,
                                          NOT_USER_TRIGGERED)
    self.assertEqual(NOT_USER_TRIGGERED_EXPECTED_XML, xml_result)

  def testBasicSuffix(self):
    original_xml = """
    <actions>
    <action name="action1">
      <owner>name1@chromium.org</owner>
      <description>Description.</description>
    </action>
    <action-suffix separator="_">
      <suffix name="suffix1" label="Suffix Description 1." />
      <affected-action name="action1" />
    </action-suffix>
    </actions>
    """
    xml_result = self._ExpandSuffixesInActionsXML(original_xml)
    self.assertMultiLineEqual(BASIC_SUFFIX_EXPECTED_XML, xml_result)

  def testSuffixPrettyPrint(self):
    """Tests that suffixes are preserved when pretty-printing."""
    original_xml = """
    <actions>
    <action name="action1">
      <owner>name1@chromium.org</owner>
      <description>Description.</description>
    </action>
    <action name="action2">
      <owner>name1@chromium.org</owner>
      <description>Description.</description>
    </action>
    <action-suffix separator="_">
      <suffix name="suffix1" label="Suffix Description 1." />
      <affected-action name="action2"/>
      <suffix name="suffix2" label="Suffix Description 2."/>
      <affected-action name="action1" />
    </action-suffix>
    </actions>
    """
    xml_result = self._PrettyPrintActionsXML(original_xml)
    expected_pretty_xml = """<actions>

<action name="action1">
  <owner>name1@chromium.org</owner>
  <description>Description.</description>
</action>

<action name="action2">
  <owner>name1@chromium.org</owner>
  <description>Description.</description>
</action>

<action-suffix separator="_">
  <suffix name="suffix1" label="Suffix Description 1."/>
  <suffix name="suffix2" label="Suffix Description 2."/>
  <affected-action name="action1"/>
  <affected-action name="action2"/>
</action-suffix>

</actions>
"""
    self.assertMultiLineEqual(expected_pretty_xml, xml_result)

  def testMultiActionMultiSuffixChain(self):
    original_xml = """
    <actions>
    <action name="action1">
      <owner>name1@chromium.org</owner>
      <description>Description.</description>
    </action>
      <action name="action2">
      <owner>name2@chromium.org</owner>
      <description>Description.</description>
    </action>
    <action-suffix separator="_">
      <suffix name="suffix1" label="Suffix Description 1." />
      <suffix name="suffix2" label="Suffix Description 2." />
      <affected-action name="action1" />
      <affected-action name="action2" />
    </action-suffix>
    <action-suffix separator="_">
      <suffix name="suffix3" label="Suffix Description 3." />
      <affected-action name="action1_suffix2" />
    </action-suffix>
    <action-suffix separator="_">
      <suffix name="suffix4" label="Suffix Description 4." />
      <affected-action name="action1_suffix2_suffix3" />
    </action-suffix>
    </actions>
    """
    xml_result = self._ExpandSuffixesInActionsXML(original_xml)
    self.assertMultiLineEqual(MULTI_ACTION_MULTI_SUFFIX_CHAIN, xml_result)

  def testSuffixCustomSeparator(self):
    original_xml = """
    <actions>
    <action name="action1">
      <owner>name1@chromium.org</owner>
      <description>Description.</description>
    </action>
    <action-suffix separator=".">
      <suffix name="suffix1" label="Suffix Description 1." />
      <affected-action name="action1" />
    </action-suffix>
    </actions>
    """
    xml_result = self._ExpandSuffixesInActionsXML(original_xml)
    self.assertMultiLineEqual(SUFFIX_CUSTOM_SEPARATOR, xml_result)

  def testSuffixOrderingPrefix(self):
    original_xml = """
    <actions>
    <action name="action1.remainder">
      <owner>name1@chromium.org</owner>
      <description>Description.</description>
    </action>
    <action-suffix ordering="prefix" separator="_">
      <suffix name="prefix1" label="Prefix Description 1." />
      <affected-action name="action1.remainder" />
    </action-suffix>
    </actions>
    """
    xml_result = self._ExpandSuffixesInActionsXML(original_xml)
    self.assertMultiLineEqual(SUFFIX_OREDERING_PREFIX, xml_result)

  def testAffectedActionWithSuffixTag(self):
    original_xml = """
    <actions>
    <action name="action1">
      <owner>name1@chromium.org</owner>
      <description>Description.</description>
    </action>
      <action name="action2">
      <owner>name2@chromium.org</owner>
      <description>Description.</description>
    </action>
    <action-suffix separator="_">
      <suffix name="suffix1" label="Suffix Description 1." />
      <suffix name="suffix2" label="Suffix Description 2." />
      <affected-action name="action1" />
      <affected-action name="action2" >
        <with-suffix name="suffix2" />
      </affected-action>
    </action-suffix>
    </actions>
    """
    xml_result = self._ExpandSuffixesInActionsXML(original_xml)
    self.assertMultiLineEqual(AFFECTED_ACTION_WITH_SUFFIX_TAG, xml_result)

  def testErrorActionMissing(self):
    original_xml = """
    <actions>
    <action name="action1">
      <owner>name1@chromium.org</owner>
      <description>Description.</description>
    </action>
    <action-suffix>
      <suffix name="suffix1" label="Suffix Description 1." />
      <affected-action name="action1" />
      <affected-action name="action2" />
    </action-suffix>
    </actions>
    """
    with self.assertRaises(action_utils.UndefinedActionItemError) as cm:
      extract_actions.ParseActionFile(original_xml)

  def testErrorSuffixNameMissing(self):
    original_xml = """
    <actions>
    <action name="action1">
      <owner>name1@chromium.org</owner>
      <description>Description.</description>
    </action>
    <action-suffix>
      <suffix label="Suffix Description 1." />
      <affected-action name="action1" />
    </action-suffix>
    </actions>
    """
    with self.assertRaises(action_utils.SuffixNameEmptyError) as cm:
      extract_actions.ParseActionFile(original_xml)

  def testErrorBadActionName(self):
    original_xml = """
    <actions>
    <action name="action1">
      <owner>name1@chromium.org</owner>
      <description>Description.</description>
    </action>
    <action-suffix ordering="prefix">
      <suffix name="prefix1" label="Prefix Description 1." />
      <affected-action name="action1" />
    </action-suffix>
    </actions>
    """
    with self.assertRaises(action_utils.InvalidAffecteddActionNameError) as cm:
      extract_actions.ParseActionFile(original_xml)

  def testUserMetricsActionSpanningTwoLines(self):
    code = 'base::UserMetricsAction(\n"Foo.Bar"));'
    finder = extract_actions.ActionNameFinder('dummy', code,
        extract_actions.USER_METRICS_ACTION_RE)
    self.assertEqual('Foo.Bar', finder.FindNextAction())
    self.assertFalse(finder.FindNextAction())

  def testUserMetricsActionAsAParam(self):
    code = 'base::UserMetricsAction("Test.Foo"), "Test.Bar");'
    finder = extract_actions.ActionNameFinder('dummy', code,
        extract_actions.USER_METRICS_ACTION_RE)
    self.assertEqual('Test.Foo', finder.FindNextAction())
    self.assertFalse(finder.FindNextAction())

  def testNonLiteralUserMetricsAction(self):
    code = 'base::UserMetricsAction(FOO)'
    finder = extract_actions.ActionNameFinder('dummy', code,
        extract_actions.USER_METRICS_ACTION_RE)
    with self.assertRaises(Exception):
      finder.FindNextAction()

  def testTernaryUserMetricsAction(self):
    code = 'base::UserMetricsAction(foo ? "Foo.Bar" : "Bar.Foo"));'
    finder = extract_actions.ActionNameFinder('dummy', code,
        extract_actions.USER_METRICS_ACTION_RE)
    with self.assertRaises(Exception):
      finder.FindNextAction()

  def testTernaryUserMetricsActionWithNewLines(self):
    code = """base::UserMetricsAction(
      foo_bar ? "Bar.Foo" :
      "Foo.Car")"""
    finder = extract_actions.ActionNameFinder('dummy', code,
        extract_actions.USER_METRICS_ACTION_RE)
    with self.assertRaises(extract_actions.InvalidStatementException):
      finder.FindNextAction()

  def testUserMetricsActionWithExtraWhitespace(self):
    code = """base::UserMetricsAction("Foo.Bar" )"""
    finder = extract_actions.ActionNameFinder('dummy', code,
        extract_actions.USER_METRICS_ACTION_RE)
    with self.assertRaises(extract_actions.InvalidStatementException):
      finder.FindNextAction()

  def testUserMetricsActionSpanningTwoLinesJs(self):
    code = "chrome.send('coreOptionsUserMetricsAction',\n['Foo.Bar']);"
    finder = extract_actions.ActionNameFinder('dummy', code,
        extract_actions.USER_METRICS_ACTION_RE_JS)
    self.assertEqual('Foo.Bar', finder.FindNextAction())
    self.assertFalse(finder.FindNextAction())

  def testNonLiteralUserMetricsActionJs(self):
    code = "chrome.send('coreOptionsUserMetricsAction',\n[FOO]);"
    finder = extract_actions.ActionNameFinder('dummy', code,
        extract_actions.USER_METRICS_ACTION_RE_JS)
    self.assertFalse(finder.FindNextAction())

  def testTernaryUserMetricsActionJs(self):
    code = ("chrome.send('coreOptionsUserMetricsAction', "
            "[foo ? 'Foo.Bar' : 'Bar.Foo']);")
    finder = extract_actions.ActionNameFinder('dummy', code,
        extract_actions.USER_METRICS_ACTION_RE_JS)
    self.assertFalse(finder.FindNextAction())

  def testTernaryUserMetricsActionWithNewLinesJs(self):
    code = """chrome.send('coreOptionsUserMetricsAction',
      [foo ? 'Foo.Bar' :
      'Bar.Foo']);"""
    finder = extract_actions.ActionNameFinder('dummy', code,
        extract_actions.USER_METRICS_ACTION_RE_JS)
    self.assertFalse(finder.FindNextAction())

  def testUserMetricsActionWithExtraCharactersJs(self):
    code = """chrome.send('coreOptionsUserMetricsAction',
      ['Foo.Bar' + 1]);"""
    finder = extract_actions.ActionNameFinder('dummy', code,
        extract_actions.USER_METRICS_ACTION_RE_JS)
    self.assertFalse(finder.FindNextAction())

  def testComputedUserMetricsActionJs(self):
    code = """chrome.send('coreOptionsUserMetricsAction',
      ['Foo.' + foo_bar ? 'Bar' : 'Foo']);"""
    finder = extract_actions.ActionNameFinder('dummy', code,
        extract_actions.USER_METRICS_ACTION_RE_JS)
    self.assertFalse(finder.FindNextAction())

  def testUserMetricsActionWithMismatchedQuotes(self):
    code = "chrome.send('coreOptionsUserMetricsAction', [\"Foo.Bar']);"
    finder = extract_actions.ActionNameFinder('dummy', code,
        extract_actions.USER_METRICS_ACTION_RE_JS)
    self.assertFalse(finder.FindNextAction())

  def testUserMetricsActionFromPropertyJs(self):
    code = "chrome.send('coreOptionsUserMetricsAction', [objOrArray[key]]);"
    finder = extract_actions.ActionNameFinder('dummy', code,
        extract_actions.USER_METRICS_ACTION_RE_JS)
    self.assertFalse(finder.FindNextAction())

  def testUserMetricsActionFromFunctionJs(self):
    code = "chrome.send('coreOptionsUserMetricsAction', [getAction(param)]);"
    finder = extract_actions.ActionNameFinder('dummy', code,
        extract_actions.USER_METRICS_ACTION_RE_JS)
    self.assertFalse(finder.FindNextAction())


if __name__ == '__main__':
  unittest.main()
