#!/usr/bin/env python3
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from typing import List, Any
from dataclasses import dataclass
from parameterized import parameterized

import setup_modules

import chromium_src.tools.metrics.actions.action_utils as action_utils
import chromium_src.tools.metrics.actions.extract_actions as extract_actions


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

XML_WITH_TOKEN = """<actions>
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

XML_WITH_TOKEN_PRETTY_PRINTED = """<actions>

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


class TestActionXmlValidation(unittest.TestCase):

  def testTwoObsoletes(self):
    current_xml = ACTIONS_XML.format(owners=TWO_OWNERS,
                                     obsolete=TWO_OBSOLETE,
                                     description=DESCRIPTION,
                                     comment=NO_VALUE,
                                     not_user_triggered=NO_VALUE)

    # Since there are two obsolete tags, the function ParseActionFile will
    # raise ValueError.
    with self.assertRaises(ValueError) as error_ctx:
      action_utils.ParseActionFile(current_xml)
    self.assertTrue('obsolete' in str(error_ctx.exception))

  def testTwoDescriptions(self):
    current_xml = ACTIONS_XML.format(owners=TWO_OWNERS,
                                     obsolete=NO_VALUE,
                                     description=TWO_DESCRIPTIONS,
                                     comment=NO_VALUE,
                                     not_user_triggered=NO_VALUE)
    # Since there are two description tags, the function ParseActionFile will
    # raise ValueError.
    with self.assertRaises(ValueError) as error_ctx:
      action_utils.ParseActionFile(current_xml)
    self.assertTrue('description' in str(error_ctx.exception))


class TestActionXmlPrettyPrint(unittest.TestCase):

  @dataclass(frozen=True)
  class _TestScenario:
    # Input
    owner: str
    description: str
    obsolete: str
    not_user_triggered: str
    generated_actions: List[str]
    comment: str

    # Expectations
    expected_xml: str

    @classmethod
    def Create(cls,
               owner: str = NO_VALUE,
               description: str = NO_VALUE,
               obsolete: str = NO_VALUE,
               not_user_triggered=NO_VALUE,
               generated_actions: List[str] = [],
               comment: str = NO_VALUE,
               expected_xml=NO_VALUE) -> "TestCase":
      return cls(owner=owner,
                 description=description,
                 obsolete=obsolete,
                 not_user_triggered=not_user_triggered,
                 generated_actions=generated_actions,
                 comment=comment,
                 expected_xml=expected_xml)

  @parameterized.expand([
      ("testNoOwner",
       _TestScenario.Create(description=DESCRIPTION,
                            expected_xml=NO_OWNER_EXPECTED_XML)),
      ("testOneOwnerOneDescription",
       _TestScenario.Create(owner=ONE_OWNER,
                            description=DESCRIPTION,
                            expected_xml=ONE_OWNER_EXPECTED_XML)),
      ("testTwoOwners",
       _TestScenario.Create(owner=TWO_OWNERS,
                            description=DESCRIPTION,
                            expected_xml=TWO_OWNERS_EXPECTED_XML)),
      ("testNoDescription",
       _TestScenario.Create(owner=TWO_OWNERS,
                            expected_xml=NO_DESCRIPTION_EXPECTED_XML)),
      ("testObsolete",
       _TestScenario.Create(owner=TWO_OWNERS,
                            description=DESCRIPTION,
                            obsolete=OBSOLETE,
                            expected_xml=OBSOLETE_EXPECTED_XML)),
      ("testGeneratedNewActions",
       _TestScenario.Create(owner=TWO_OWNERS,
                            description=DESCRIPTION,
                            generated_actions=['action2'],
                            expected_xml=ADD_ACTION_EXPECTED_XML)),
      ("testComment",
       _TestScenario.Create(owner=TWO_OWNERS,
                            description=DESCRIPTION,
                            comment=COMMENT,
                            expected_xml=COMMENT_EXPECTED_XML)),
      ("testNotUserTriggered",
       _TestScenario.Create(description=DESCRIPTION,
                            not_user_triggered=NOT_USER_TRIGGERED,
                            expected_xml=NOT_USER_TRIGGERED_EXPECTED_XML))
  ])
  def testUpdateXml(self, _, test_scenario: _TestScenario):
    input_xml = ACTIONS_XML.format(
        owners=test_scenario.owner,
        description=test_scenario.description,
        obsolete=test_scenario.obsolete,
        comment=test_scenario.comment,
        not_user_triggered=test_scenario.not_user_triggered)
    updated_xml = extract_actions.UpdateXml(
        input_xml, generated_actions_names=test_scenario.generated_actions)
    self.assertEqual(updated_xml, test_scenario.expected_xml)

  def testVariantPrettyPrint(self):
    """Tests that tokens and variants are preserved when pretty-printing."""
    xml_result = extract_actions.UpdateXml(XML_WITH_TOKEN,
                                           generated_actions_names=[])
    self.assertMultiLineEqual(XML_WITH_TOKEN_PRETTY_PRINTED, xml_result)


class ExtractActionsTest(unittest.TestCase):

  @parameterized.expand([
      ('testUserMetricsActionSpanningTwoLines',
       'base::UserMetricsAction(\n"Foo.Bar"));', ['Foo.Bar']),
      ('testUserMetricsActionAsAParam',
       'base::UserMetricsAction("Test.Foo"), "Test.Bar");', ['Test.Foo']),
      ('testNonLiteralUserMetricsAction', 'base::UserMetricsAction(FOO)', []),
      ('testTernaryUserMetricsAction',
       'base::UserMetricsAction(foo ? "Foo.Bar" : "Bar.Foo"));', []),
      ('testTernaryUserMetricsActionWithNewLines', """base::UserMetricsAction(
      foo_bar ? "Bar.Foo" :
      "Foo.Car")""", []),
      ('testUserMetricsActionWithExtraWhitespace',
       'base::UserMetricsAction("Foo.Bar" )', ['Foo.Bar']),
      ('testUserMetricsActionWithStringConcatenation',
       'base::UserMetricsAction("Foo.Bar" "Baz.Qux")', ['Foo.BarBaz.Qux']),
      ('testUserMetricsActionWithStringConcatenationWithPlus',
       'base::UserMetricsAction("Foo.Bar" + "Baz.Qux")', []),
      ('testUserMetricsActionWithEscapedQuotes',
       'base::UserMetricsAction("Foo.Bar\\"Baz")', ['Foo.Bar"Baz']),
      ('testUserMetricsActionWithMixedQuotes',
       """base::UserMetricsAction('Foo."Bar"' )""", ['Foo."Bar"'])
  ])
  def testActionNameFinder(
      self,
      _,
      code: str,
      expected_actions: List[Any],
  ):
    finder = extract_actions.ActionNameFinder(
        'dummy', code, extract_actions.USER_METRICS_ACTION_RE)
    for expected_action in expected_actions:
      self.assertEqual(finder.FindNextAction(), expected_action)
    self.assertIsNone(finder.FindNextAction())

  @parameterized.expand([
      ('testUserMetricsActionSpanningTwoLinesJs',
       "chrome.send('coreOptionsUserMetricsAction',\n['Foo.Bar']);",
       ['Foo.Bar']),
      ('testNonLiteralUserMetricsActionJs',
       "chrome.send('coreOptionsUserMetricsAction',\n[FOO]);", []),
      ('testTernaryUserMetricsActionJs',
       ("chrome.send('coreOptionsUserMetricsAction', "
        "[foo ? 'Foo.Bar' : 'Bar.Foo']);"), []),
      ('testTernaryUserMetricsActionWithNewLinesJs',
       """chrome.send('coreOptionsUserMetricsAction',
      [foo ? 'Foo.Bar' :
      'Bar.Foo']);""", []),
      ('testUserMetricsActionWithExtraCharactersJs',
       """chrome.send('coreOptionsUserMetricsAction',
      ['Foo.Bar' + 1]);""", []),
      ('testComputedUserMetricsActionJs',
       """chrome.send('coreOptionsUserMetricsAction',
      ['Foo.' + foo_bar ? 'Bar' : 'Foo']);""", []),
      ('testUserMetricsActionWithMismatchedQuotes',
       "chrome.send('coreOptionsUserMetricsAction', [\"Foo.Bar']);", []),
      ('testUserMetricsActionFromPropertyJs',
       "chrome.send('coreOptionsUserMetricsAction', [objOrArray[key]]);", []),
      ('testUserMetricsActionFromFunctionJs',
       "chrome.send('coreOptionsUserMetricsAction', [getAction(param)]);", [])
  ])
  def testActionNameFinderJs(self, _, code: str, expected_actions: List[Any]):
    finder = extract_actions.ActionNameFinder(
        'dummy', code, extract_actions.USER_METRICS_ACTION_RE_JS)
    for expected_action in expected_actions:
      self.assertEqual(finder.FindNextAction(), expected_action)
    self.assertIsNone(finder.FindNextAction())

if __name__ == '__main__':
  unittest.main()
