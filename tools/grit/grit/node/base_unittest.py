#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for base.Node functionality (as used in various subclasses)'''

import io
import os
import sys
import unittest

if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

from grit import grd_reader
from grit import util
from grit.node import base
from grit.node import message


def MakePlaceholder(phname='BINGO'):
  ph = message.PhNode()
  ph.StartParsing('ph', None)
  ph.HandleAttribute('name', phname)
  ph.AppendContent('bongo')
  ph.EndParsing()
  return ph


class NodeUnittest(unittest.TestCase):
  def testWhitespaceHandling(self):
    # We test using the Message node type.
    node = message.MessageNode()
    node.StartParsing('hello', None)
    node.HandleAttribute('name', 'bla')
    node.AppendContent(" '''  two spaces  ")
    node.EndParsing()
    self.assertTrue(node.GetCdata() == '  two spaces')

    node = message.MessageNode()
    node.StartParsing('message', None)
    node.HandleAttribute('name', 'bla')
    node.AppendContent("  two spaces  '''  ")
    node.EndParsing()
    self.assertTrue(node.GetCdata() == 'two spaces  ')

  def testWhitespaceHandlingWithChildren(self):
    # We test using the Message node type.
    node = message.MessageNode()
    node.StartParsing('message', None)
    node.HandleAttribute('name', 'bla')
    node.AppendContent(" '''  two spaces  ")
    node.AddChild(MakePlaceholder())
    node.AppendContent(' space before and after ')
    node.AddChild(MakePlaceholder('BONGO'))
    node.AppendContent(" space before two after  '''")
    node.EndParsing()
    self.assertTrue(node.mixed_content[0] == '  two spaces  ')
    self.assertTrue(node.mixed_content[2] == ' space before and after ')
    self.assertTrue(node.mixed_content[-1] == ' space before two after  ')

  def testXmlFormatMixedContent(self):
    # Again test using the Message node type, because it is the only mixed
    # content node.
    node = message.MessageNode()
    node.StartParsing('message', None)
    node.HandleAttribute('name', 'name')
    node.AppendContent('Hello <young> ')

    ph = message.PhNode()
    ph.StartParsing('ph', None)
    ph.HandleAttribute('name', 'USERNAME')
    ph.AppendContent('$1')
    ex = message.ExNode()
    ex.StartParsing('ex', None)
    ex.AppendContent('Joi')
    ex.EndParsing()
    ph.AddChild(ex)
    ph.EndParsing()

    node.AddChild(ph)
    node.EndParsing()

    non_indented_xml = node.FormatXml()
    self.assertTrue(non_indented_xml == '<message name="name">\n  Hello '
                    '&lt;young&gt; <ph name="USERNAME">$1<ex>Joi</ex></ph>'
                    '\n</message>')

    indented_xml = node.FormatXml('  ')
    self.assertTrue(indented_xml == '  <message name="name">\n    Hello '
                    '&lt;young&gt; <ph name="USERNAME">$1<ex>Joi</ex></ph>'
                    '\n  </message>')

  def testXmlFormatMixedContentWithLeadingWhitespace(self):
    # Again test using the Message node type, because it is the only mixed
    # content node.
    node = message.MessageNode()
    node.StartParsing('message', None)
    node.HandleAttribute('name', 'name')
    node.AppendContent("'''   Hello <young> ")

    ph = message.PhNode()
    ph.StartParsing('ph', None)
    ph.HandleAttribute('name', 'USERNAME')
    ph.AppendContent('$1')
    ex = message.ExNode()
    ex.StartParsing('ex', None)
    ex.AppendContent('Joi')
    ex.EndParsing()
    ph.AddChild(ex)
    ph.EndParsing()

    node.AddChild(ph)
    node.AppendContent(" yessiree '''")
    node.EndParsing()

    non_indented_xml = node.FormatXml()
    self.assertTrue(non_indented_xml ==
                    "<message name=\"name\">\n  '''   Hello"
                    ' &lt;young&gt; <ph name="USERNAME">$1<ex>Joi</ex></ph>'
                    " yessiree '''\n</message>")

    indented_xml = node.FormatXml('  ')
    self.assertTrue(indented_xml ==
                    "  <message name=\"name\">\n    '''   Hello"
                    ' &lt;young&gt; <ph name="USERNAME">$1<ex>Joi</ex></ph>'
                    " yessiree '''\n  </message>")

    self.assertTrue(node.GetNodeById('name'))

  def testXmlFormatContentWithEntities(self):
    '''Tests a bug where &nbsp; would not be escaped correctly.'''
    from grit import tclib
    msg_node = message.MessageNode.Construct(None, tclib.Message(
      text = 'BEGIN_BOLDHelloWHITESPACEthere!END_BOLD Bingo!',
      placeholders = [
        tclib.Placeholder('BEGIN_BOLD', '<b>', 'bla'),
        tclib.Placeholder('WHITESPACE', '&nbsp;', 'bla'),
        tclib.Placeholder('END_BOLD', '</b>', 'bla')]),
                                             'BINGOBONGO')
    xml = msg_node.FormatXml()
    self.assertTrue(xml.find('&nbsp;') == -1, 'should have no entities')

  def testIter(self):
    # First build a little tree of message and ph nodes.
    node = message.MessageNode()
    node.StartParsing('message', None)
    node.HandleAttribute('name', 'bla')
    node.AppendContent(" '''  two spaces  ")
    node.AppendContent(' space before and after ')
    ph = message.PhNode()
    ph.StartParsing('ph', None)
    ph.AddChild(message.ExNode())
    ph.HandleAttribute('name', 'BINGO')
    ph.AppendContent('bongo')
    node.AddChild(ph)
    node.AddChild(message.PhNode())
    node.AppendContent(" space before two after  '''")

    order = [
        message.MessageNode, message.PhNode, message.ExNode, message.PhNode
    ]
    for n in node:
      self.assertTrue(type(n) == order[0])
      order = order[1:]
    self.assertTrue(len(order) == 0)

  def testGetChildrenOfType(self):
    xml = '''<?xml version="1.0" encoding="UTF-8"?>
      <grit latest_public_release="2" source_lang_id="en-US"
            current_release="3" base_dir=".">
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
                           util.PathFromRoot('grit/test/data'))
    from grit.node import node_io
    output_nodes = grd.GetChildrenOfType(node_io.OutputNode)
    self.assertEqual(len(output_nodes), 3)
    self.assertEqual(output_nodes[2].attrs['filename'],
                         'de/generated_resources.rc')

  def testEvaluateExpression(self):
    def AssertExpr(expected_value, expr, defs, target_platform,
                   extra_variables):
      self.assertEqual(expected_value, base.Node.EvaluateExpression(
          expr, defs, target_platform, extra_variables))

    AssertExpr(True, "True", {}, 'linux', {})
    AssertExpr(False, "False", {}, 'linux', {})
    AssertExpr(True, "True or False", {}, 'linux', {})
    AssertExpr(False, "True and False", {}, 'linux', {})
    AssertExpr(True, "os == 'linux'", {}, 'linux', {})
    AssertExpr(False, "os == 'linux'", {}, 'ios', {})
    AssertExpr(True, "'foo' in defs", {'foo': 'bar'}, 'ios', {})
    AssertExpr(False, "'foo' in defs", {'baz': 'bar'}, 'ios', {})
    AssertExpr(False, "'foo' in defs", {}, 'ios', {})
    AssertExpr(True, "is_linux", {}, 'linux', {})
    AssertExpr(False, "is_linux", {}, 'linux2', {})  # Python 2 used 'linux2'.
    AssertExpr(False, "is_linux", {}, 'linux-foo', {})  # Must match exactly.
    AssertExpr(False, "is_linux", {}, 'foollinux', {})
    AssertExpr(False, "is_linux", {'chromeos_ash': True}, 'chromeos', {})
    AssertExpr(False, "is_linux", {'chromeos_lacros': True}, 'chromeos', {})
    # `is_chromeos` is not used with GRIT and is thus ignored.
    AssertExpr(True, "is_linux", {'is_chromeos': True}, 'linux', {})
    AssertExpr(True, "is_chromeos", {'chromeos_ash': True}, 'chromeos', {})
    AssertExpr(True, "is_chromeos", {'chromeos_lacros': True}, 'chromeos', {})
    AssertExpr(False, "is_chromeos", {}, 'linux', {})
    AssertExpr(False, "is_fuchsia", {}, 'linux', {})
    AssertExpr(False, "is_linux", {}, 'win32', {})
    AssertExpr(True, "is_macosx", {}, 'darwin', {})
    AssertExpr(False, "is_macosx", {}, 'ios', {})
    AssertExpr(True, "is_win", {}, 'win32', {})
    AssertExpr(False, "is_win", {}, 'darwin', {})
    AssertExpr(True, "is_android", {}, 'android', {})
    AssertExpr(False, "is_android", {}, 'linux3', {})
    AssertExpr(True, "is_ios", {}, 'ios', {})
    AssertExpr(False, "is_ios", {}, 'darwin', {})
    AssertExpr(True, "is_posix", {}, 'linux', {})
    AssertExpr(True, "is_posix", {'chromeos_ash': True}, 'chromeos', {})
    AssertExpr(True, "is_posix", {'chromeos_lacros': True}, 'chromeos', {})
    AssertExpr(True, "is_posix", {}, 'darwin', {})
    AssertExpr(True, "is_posix", {}, 'android', {})
    AssertExpr(True, "is_posix", {}, 'ios', {})
    AssertExpr(True, "is_posix", {}, 'freebsd7', {})
    AssertExpr(False, "is_posix", {}, 'fuchsia', {})
    AssertExpr(False, "is_posix", {}, 'win32', {})
    AssertExpr(True, "is_fuchsia", {}, 'fuchsia', {})
    AssertExpr(True, "pp_ifdef('foo')", {'foo': True}, 'win32', {})
    AssertExpr(True, "pp_ifdef('foo')", {'foo': False}, 'win32', {})
    AssertExpr(False, "pp_ifdef('foo')", {'bar': True}, 'win32', {})
    AssertExpr(True, "pp_if('foo')", {'foo': True}, 'win32', {})
    AssertExpr(False, "pp_if('foo')", {'foo': False}, 'win32', {})
    AssertExpr(False, "pp_if('foo')", {'bar': True}, 'win32', {})
    AssertExpr(True, "foo", {'foo': True}, 'win32', {})
    AssertExpr(False, "foo", {'foo': False}, 'win32', {})
    AssertExpr(False, "foo", {'bar': True, 'foo': False}, 'win32', {})
    AssertExpr(True, "foo == 'baz'", {'foo': 'baz'}, 'win32', {})
    AssertExpr(False, "foo == 'baz'", {'foo': True}, 'win32', {})
    AssertExpr(False, "foo == 'baz'", {'foo': True, 'baz': False}, 'win32', {})
    AssertExpr(True, "lang == 'de'", {}, 'win32', {'lang': 'de'})
    AssertExpr(False, "lang == 'de'", {}, 'win32', {'lang': 'fr'})

    # Test a couple more complex expressions for good measure.
    AssertExpr(True, "is_ios and (lang in ['de', 'fr'] or foo)",
               {'foo': 'bar'}, 'ios', {'lang': 'fr', 'context': 'today'})
    AssertExpr(False, "is_ios and (lang in ['de', 'fr'] or foo)",
               {'foo': False}, 'linux', {
                   'lang': 'fr',
                   'context': 'today'
               })
    AssertExpr(False, "is_ios and (lang in ['de', 'fr'] or foo)", {
        'baz': 'bar',
        'is_ios': True,
        'foo': False
    }, 'ios', {
        'lang': 'he',
        'context': 'today'
    })
    AssertExpr(True, "foo == 'bar' or not baz", {
        'foo': 'bar',
        'baz': True
    }, 'ios', {
        'lang': 'en',
        'context': 'java'
    })
    AssertExpr(False, "foo == 'bar' or not baz",
               {'foo': 'ruz', 'baz': True}, 'ios', {'lang': 'en'})

  def testEvaluateExpressionThrows(self):
    def AssertThrows(expr, defs, target_platform, message):
      with self.assertRaises(AssertionError) as cm:
        base.Node.EvaluateExpression(expr, defs, target_platform, {})
      self.assertTrue(str(cm.exception) == message)

    # Test undefined variables.
    AssertThrows("foo == 'baz'", {}, 'linux',
                 'undefined Grit variable found: foo')
    AssertThrows("foo == 'bar' or not baz", {
        'foo': 'bar',
        'fun': True
    }, 'linux', 'undefined Grit variable found: baz')

    # Test invalid chromeos configurations.
    AssertThrows("is_chromeos", {}, 'chromeos',
                 'The chromeos target must be either ash or lacros')
    AssertThrows("is_chromeos", {
        'chromeos_ash': True,
        'chromeos_lacros': True
    }, 'chromeos', 'The chromeos target must be either ash or lacros')
    AssertThrows("is_linux", {'chromeos_ash': True}, 'linux',
                 'Non-chromeos targets cannot be ash or lacros')
    AssertThrows("is_linux", {'chromeos_lacros': True}, 'linux',
                 'Non-chromeos targets cannot be ash or lacros')
    AssertThrows("is_linux", {
        'chromeos_ash': True,
        'chromeos_lacros': True
    }, 'linux', 'Non-chromeos targets cannot be ash or lacros')


if __name__ == '__main__':
  unittest.main()
