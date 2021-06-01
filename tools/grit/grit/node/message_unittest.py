#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.node.message'''

from __future__ import print_function

import os
import sys
import unittest

if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

from grit import exception
from grit import tclib
from grit import util
from grit.node import message

class MessageUnittest(unittest.TestCase):
  def testMessage(self):
    root = util.ParseGrdForUnittest('''
        <messages>
        <message name="IDS_GREETING"
                 desc="Printed to greet the currently logged in user">
        Hello <ph name="USERNAME">%s<ex>Joi</ex></ph>, how are you doing today?
        </message>
        </messages>''')
    msg, = root.GetChildrenOfType(message.MessageNode)
    cliques = msg.GetCliques()
    content = cliques[0].GetMessage().GetPresentableContent()
    self.failUnless(content == 'Hello USERNAME, how are you doing today?')

  def testMessageWithWhitespace(self):
    root = util.ParseGrdForUnittest("""\
        <messages>
        <message name="IDS_BLA" desc="">
        '''  Hello there <ph name="USERNAME">%s</ph>   '''
        </message>
        </messages>""")
    msg, = root.GetChildrenOfType(message.MessageNode)
    content = msg.GetCliques()[0].GetMessage().GetPresentableContent()
    self.failUnless(content == 'Hello there USERNAME')
    self.failUnless(msg.ws_at_start == '  ')
    self.failUnless(msg.ws_at_end == '   ')

  def testConstruct(self):
    msg = tclib.Message(text="   Hello USERNAME, how are you?   BINGO\t\t",
                        placeholders=[tclib.Placeholder('USERNAME', '%s', 'Joi'),
                                      tclib.Placeholder('BINGO', '%d', '11')])
    msg_node = message.MessageNode.Construct(None, msg, 'BINGOBONGO')
    self.failUnless(msg_node.children[0].name == 'ph')
    self.failUnless(msg_node.children[0].children[0].name == 'ex')
    self.failUnless(msg_node.children[0].children[0].GetCdata() == 'Joi')
    self.failUnless(msg_node.children[1].children[0].GetCdata() == '11')
    self.failUnless(msg_node.ws_at_start == '   ')
    self.failUnless(msg_node.ws_at_end == '\t\t')

  def testUnicodeConstruct(self):
    text = u'Howdie \u00fe'
    msg = tclib.Message(text=text)
    msg_node = message.MessageNode.Construct(None, msg, 'BINGOBONGO')
    msg_from_node = msg_node.GetCdata()
    self.failUnless(msg_from_node == text)

  def testFormatterData(self):
    root = util.ParseGrdForUnittest("""\
        <messages>
        <message name="IDS_BLA" desc="" formatter_data="  foo=123 bar  qux=low">
          Text
        </message>
        </messages>""")
    msg, = root.GetChildrenOfType(message.MessageNode)
    expected_formatter_data = {
        'foo': '123',
        'bar': '',
        'qux': 'low'}

    # Can't use assertDictEqual, not available in Python 2.6, so do it
    # by hand.
    self.failUnlessEqual(len(expected_formatter_data),
                         len(msg.formatter_data))
    for key in expected_formatter_data:
      self.failUnlessEqual(expected_formatter_data[key],
                           msg.formatter_data[key])

  def testReplaceEllipsis(self):
    root = util.ParseGrdForUnittest('''
        <messages>
        <message name="IDS_GREETING" desc="">
        A...B.... <ph name="PH">%s<ex>A</ex></ph>... B... C...
        </message>
        </messages>''')
    msg, = root.GetChildrenOfType(message.MessageNode)
    msg.SetReplaceEllipsis(True)
    content = msg.Translate('en')
    self.failUnlessEqual(u'A...B.... %s\u2026 B\u2026 C\u2026', content)

  def testRemoveByteOrderMark(self):
    root = util.ParseGrdForUnittest(u'''
        <messages>
        <message name="IDS_HAS_BOM" desc="">
        \uFEFFThis\uFEFF i\uFEFFs OK\uFEFF
        </message>
        </messages>''')
    msg, = root.GetChildrenOfType(message.MessageNode)
    content = msg.Translate('en')
    self.failUnlessEqual(u'This is OK', content)

  def testPlaceholderHasTooManyExamples(self):
    try:
      util.ParseGrdForUnittest("""\
        <messages>
        <message name="IDS_FOO" desc="foo">
          Hi <ph name="NAME">$1<ex>Joi</ex><ex>Joy</ex></ph>
        </message>
        </messages>""")
    except exception.TooManyExamples:
      return
    self.fail('Should have gotten exception')

  def testPlaceholderHasInvalidName(self):
    try:
      util.ParseGrdForUnittest("""\
        <messages>
        <message name="IDS_FOO" desc="foo">
          Hi <ph name="ABC!">$1</ph>
        </message>
        </messages>""")
    except exception.InvalidPlaceholderName:
      return
    self.fail('Should have gotten exception')

  def testChromeLocalizedFormatIsInsidePhNode(self):
    try:
      util.ParseGrdForUnittest("""\
        <messages>
        <message name="IDS_CHROME_L10N" desc="l10n format">
          This message is missing the ph node: $1
        </message>
        </messages>""")
    except exception.PlaceholderNotInsidePhNode:
      return
    self.fail('Should have gotten exception')

  def testAndroidStringFormatIsInsidePhNode(self):
    try:
      util.ParseGrdForUnittest("""\
        <messages>
        <message name="IDS_ANDROID" desc="string format">
          This message is missing a ph node: %1$s
        </message>
        </messages>""")
    except exception.PlaceholderNotInsidePhNode:
      return
    self.fail('Should have gotten exception')

  def testAndroidIntegerFormatIsInsidePhNode(self):
    try:
      util.ParseGrdForUnittest("""\
        <messages>
        <message name="IDS_ANDROID" desc="integer format">
          This message is missing a ph node: %2$d
        </message>
        </messages>""")
    except exception.PlaceholderNotInsidePhNode:
      return
    self.fail('Should have gotten exception')

  def testAndroidIntegerWidthFormatIsInsidePhNode(self):
    try:
      util.ParseGrdForUnittest("""\
        <messages>
        <message name="IDS_ANDROID" desc="integer width format">
          This message is missing a ph node: %2$3d
        </message>
        </messages>""")
    except exception.PlaceholderNotInsidePhNode:
      return
    self.fail('Should have gotten exception')

  def testValidAndroidIntegerWidthFormatInPhNode(self):
    try:
      util.ParseGrdForUnittest("""\
        <messages>
        <message name="IDS_ANDROID_WIDTH">
          <ph name="VALID">%2$3d<ex>042</ex></ph>
        </message>
        </messages>""")
    except:
      self.fail('Should not have gotten exception')

  def testAndroidFloatFormatIsInsidePhNode(self):
    try:
      util.ParseGrdForUnittest("""\
        <messages>
        <message name="IDS_ANDROID" desc="float number format">
          This message is missing a ph node: %3$4.5f
        </message>
        </messages>""")
    except exception.PlaceholderNotInsidePhNode:
      return
    self.fail('Should have gotten exception')

  def testGritStringFormatIsInsidePhNode(self):
    try:
      util.ParseGrdForUnittest("""\
        <messages>
        <message name="IDS_GRIT_STRING" desc="grit string format">
          This message is missing the ph node: %s
        </message>
        </messages>""")
    except exception.PlaceholderNotInsidePhNode:
      return
    self.fail('Should have gotten exception')

  def testGritIntegerFormatIsInsidePhNode(self):
    try:
      util.ParseGrdForUnittest("""\
        <messages>
        <message name="IDS_GRIT_INTEGER" desc="grit integer format">
          This message is missing the ph node: %d
        </message>
        </messages>""")
    except exception.PlaceholderNotInsidePhNode:
      return
    self.fail('Should have gotten exception')

  def testWindowsETWIntegerFormatIsInsidePhNode(self):
    try:
      util.ParseGrdForUnittest("""\
        <messages>
        <message name="IDS_WINDOWS_ETW" desc="ETW tracing integer">
          This message is missing the ph node: %1
        </message>
        </messages>""")
    except exception.PlaceholderNotInsidePhNode:
      return
    self.fail('Should have gotten exception')

  def testValidMultipleFormattersInsidePhNodes(self):
    root = util.ParseGrdForUnittest("""\
      <messages>
      <message name="IDS_MULTIPLE_FORMATTERS">
        <ph name="ERROR_COUNT">%1$d<ex>1</ex></ph> error, <ph name="WARNING_COUNT">%2$d<ex>1</ex></ph> warning
      </message>
      </messages>""")
    msg, = root.GetChildrenOfType(message.MessageNode)
    cliques = msg.GetCliques()
    content = cliques[0].GetMessage().GetPresentableContent()
    self.failUnless(content == 'ERROR_COUNT error, WARNING_COUNT warning')

  def testMultipleFormattersAreInsidePhNodes(self):
    failed = True
    try:
      util.ParseGrdForUnittest("""\
        <messages>
        <message name="IDS_MULTIPLE_FORMATTERS">
          %1$d error, %2$d warning
        </message>
        </messages>""")
    except exception.PlaceholderNotInsidePhNode:
      failed = False
    if failed:
      self.fail('Should have gotten exception')
      return

    failed = True
    try:
      util.ParseGrdForUnittest("""\
        <messages>
        <message name="IDS_MULTIPLE_FORMATTERS">
          <ph name="ERROR_COUNT">%1$d<ex>1</ex></ph> error, %2$d warning
        </message>
        </messages>""")
    except exception.PlaceholderNotInsidePhNode:
      failed = False
    if failed:
      self.fail('Should have gotten exception')
      return

    failed = True
    try:
      util.ParseGrdForUnittest("""\
        <messages>
        <message name="IDS_MULTIPLE_FORMATTERS">
          <ph name="INVALID">%1$d %2$d</ph>
        </message>
        </messages>""")
    except exception.InvalidCharactersInsidePhNode:
      failed = False
    if failed:
      self.fail('Should have gotten exception')
      return

  def testValidHTMLFormatInsidePhNode(self):
    try:
      util.ParseGrdForUnittest("""\
        <messages>
        <message name="IDS_HTML">
          <ph name="VALID">&lt;span&gt;$1&lt;/span&gt;<ex>1</ex></ph>
        </message>
        </messages>""")
    except:
      self.fail('Should not have gotten exception')

  def testValidHTMLWithAttributesFormatInsidePhNode(self):
    try:
      util.ParseGrdForUnittest("""\
        <messages>
        <message name="IDS_HTML_ATTRIBUTE">
          <ph name="VALID">&lt;span attribute="js:$this %"&gt;$2&lt;/span&gt;<ex>2</ex></ph>
        </message>
        </messages>""")
    except:
      self.fail('Should not have gotten exception')

  def testValidHTMLEntityFormatInsidePhNode(self):
    try:
      util.ParseGrdForUnittest("""\
        <messages>
        <message name="IDS_ENTITY">
          <ph name="VALID">&gt;%1$d&lt;<ex>1</ex></ph>
        </message>
        </messages>""")
    except:
      self.fail('Should not have gotten exception')

  def testValidMultipleDollarFormatInsidePhNode(self):
    try:
      util.ParseGrdForUnittest("""\
        <messages>
        <message name="IDS_DOLLARS" desc="l10n dollars format">
          <ph name="VALID">$$1</ph>
        </message>
        </messages>""")
    except:
      self.fail('Should not have gotten exception')

  def testInvalidDollarCharacterInsidePhNode(self):
    try:
      util.ParseGrdForUnittest("""\
        <messages>
        <message name="IDS_BAD_DOLLAR">
          <ph name="INVALID">%1$d $</ph>
        </message>
        </messages>""")
    except exception.InvalidCharactersInsidePhNode:
      return
    self.fail('Should have gotten exception')

  def testInvalidPercentCharacterInsidePhNode(self):
    try:
      util.ParseGrdForUnittest("""\
        <messages>
        <message name="IDS_BAD_PERCENT">
          <ph name="INVALID">%1$d %</ph>
        </message>
        </messages>""")
    except exception.InvalidCharactersInsidePhNode:
      return
    self.fail('Should have gotten exception')

  def testInvalidMixedFormatCharactersInsidePhNode(self):
    try:
      util.ParseGrdForUnittest("""\
        <messages>
        <message name="IDS_MIXED_FORMATS">
          <ph name="INVALID">%1$2</ph>
        </message>
        </messages>""")
    except exception.InvalidCharactersInsidePhNode:
      return
    self.fail('Should have gotten exception')


if __name__ == '__main__':
  unittest.main()
