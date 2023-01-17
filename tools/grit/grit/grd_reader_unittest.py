#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grd_reader package'''

import io
import os
import sys
import unittest

if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

from grit import exception
from grit import grd_reader
from grit import util
from grit.node import empty
from grit.node import message
from grit.exception import DuplicateKey


class GrdReaderUnittest(unittest.TestCase):
  def testParsingAndXmlOutput(self):
    input = '''<?xml version="1.0" encoding="UTF-8"?>
<grit base_dir="." current_release="3" latest_public_release="2" source_lang_id="en-US">
  <release seq="3">
    <includes>
      <include file="images/logo.gif" name="ID_LOGO" type="gif" />
    </includes>
    <messages>
      <if expr="True">
        <message desc="Printed to greet the currently logged in user" name="IDS_GREETING">
          Hello <ph name="USERNAME">%s<ex>Joi</ex></ph>, how are you doing today?
        </message>
      </if>
    </messages>
    <structures>
      <structure file="rc_files/dialogs.rc" name="IDD_NARROW_DIALOG" type="dialog">
        <skeleton expr="lang == 'fr-FR'" file="bla.rc" variant_of_revision="3" />
      </structure>
      <structure file="rc_files/version.rc" name="VS_VERSION_INFO" type="version" />
    </structures>
  </release>
  <translations>
    <file lang="nl" path="nl_translations.xtb" />
  </translations>
  <outputs>
    <output filename="resource.h" type="rc_header" />
    <output filename="resource.rc" lang="en-US" type="rc_all" />
  </outputs>
</grit>'''
    pseudo_file = io.StringIO(input)
    tree = grd_reader.Parse(pseudo_file, '.')
    output = str(tree)
    expected_output = input.replace(' base_dir="."', '')
    self.assertEqual(expected_output, output)
    self.assertTrue(tree.GetNodeById('IDS_GREETING'))


  def testStopAfter(self):
    input = '''<?xml version="1.0" encoding="UTF-8"?>
<grit latest_public_release="2" source_lang_id="en-US" current_release="3" base_dir=".">
  <outputs>
    <output filename="resource.h" type="rc_header" />
    <output filename="resource.rc" lang="en-US" type="rc_all" />
  </outputs>
  <release seq="3">
    <includes>
      <include type="gif" name="ID_LOGO" file="images/logo.gif"/>
    </includes>
  </release>
</grit>'''
    pseudo_file = io.StringIO(input)
    tree = grd_reader.Parse(pseudo_file, '.', stop_after='outputs')
    # only an <outputs> child
    self.assertTrue(len(tree.children) == 1)
    self.assertTrue(tree.children[0].name == 'outputs')

  def testLongLinesWithComments(self):
    input = '''<?xml version="1.0" encoding="UTF-8"?>
<grit latest_public_release="2" source_lang_id="en-US" current_release="3" base_dir=".">
  <release seq="3">
    <messages>
      <message name="IDS_GREETING" desc="Printed to greet the currently logged in user">
        This is a very long line with no linebreaks yes yes it stretches on <!--
        -->and on <!--
        -->and on!
      </message>
    </messages>
  </release>
</grit>'''
    pseudo_file = io.StringIO(input)
    tree = grd_reader.Parse(pseudo_file, '.')

    greeting = tree.GetNodeById('IDS_GREETING')
    self.assertTrue(greeting.GetCliques()[0].GetMessage().GetRealContent() ==
                    'This is a very long line with no linebreaks yes yes it '
                    'stretches on and on and on!')

  def doTestAssignFirstIds(self, first_ids_path):
    input = '''<?xml version="1.0" encoding="UTF-8"?>
<grit latest_public_release="2" source_lang_id="en-US" current_release="3"
      base_dir="." first_ids_file="%s">
  <release seq="3">
    <messages>
      <message name="IDS_TEST" desc="test">
        test
      </message>
    </messages>
  </release>
</grit>''' % first_ids_path
    pseudo_file = io.StringIO(input)
    grit_root_dir = os.path.join(os.path.abspath(os.path.dirname(__file__)),
                                 '..')
    fake_input_path = os.path.join(
        grit_root_dir, "grit/testdata/chrome/app/generated_resources.grd")
    root = grd_reader.Parse(pseudo_file, os.path.split(fake_input_path)[0])
    root.AssignFirstIds(fake_input_path, {})
    messages_node = root.children[0].children[0]
    self.assertTrue(isinstance(messages_node, empty.MessagesNode))
    self.assertTrue(messages_node.attrs["first_id"] !=
        empty.MessagesNode().DefaultAttributes()["first_id"])

  def testAssignFirstIds(self):
    self.doTestAssignFirstIds("../../tools/grit/resource_ids")

  def testAssignFirstIdsUseGritDir(self):
    self.doTestAssignFirstIds("GRIT_DIR/grit/testdata/tools/grit/resource_ids")

  def testAssignFirstIdsMultipleMessages(self):
    """If there are multiple messages sections, the resource_ids file
    needs to list multiple first_id values."""
    input = '''<?xml version="1.0" encoding="UTF-8"?>
<grit latest_public_release="2" source_lang_id="en-US" current_release="3"
      base_dir="." first_ids_file="resource_ids">
  <release seq="3">
    <messages>
      <message name="IDS_TEST" desc="test">
        test
      </message>
    </messages>
    <messages>
      <message name="IDS_TEST2" desc="test">
        test2
      </message>
    </messages>
  </release>
</grit>'''
    pseudo_file = io.StringIO(input)
    grit_root_dir = os.path.join(os.path.abspath(os.path.dirname(__file__)),
                                 '..')
    fake_input_path = os.path.join(grit_root_dir, "grit/testdata/test.grd")

    root = grd_reader.Parse(pseudo_file, os.path.split(fake_input_path)[0])
    root.AssignFirstIds(fake_input_path, {})
    messages_node = root.children[0].children[0]
    self.assertTrue(isinstance(messages_node, empty.MessagesNode))
    self.assertEqual('100', messages_node.attrs["first_id"])
    messages_node = root.children[0].children[1]
    self.assertTrue(isinstance(messages_node, empty.MessagesNode))
    self.assertEqual('10000', messages_node.attrs["first_id"])

  def testUseNameForIdAndPpIfdef(self):
    input = '''<?xml version="1.0" encoding="UTF-8"?>
<grit latest_public_release="2" source_lang_id="en-US" current_release="3" base_dir=".">
  <release seq="3">
    <messages>
      <if expr="pp_ifdef('hello')">
        <message name="IDS_HELLO" use_name_for_id="true">
          Hello!
        </message>
      </if>
    </messages>
  </release>
</grit>'''
    pseudo_file = io.StringIO(input)
    root = grd_reader.Parse(pseudo_file, '.', defines={'hello': '1'})

    # Check if the ID is set to the name. In the past, there was a bug
    # that caused the ID to be a generated number.
    hello = root.GetNodeById('IDS_HELLO')
    self.assertTrue(hello.GetCliques()[0].GetId() == 'IDS_HELLO')

  def testUseNameForIdWithIfElse(self):
    input = '''<?xml version="1.0" encoding="UTF-8"?>
<grit latest_public_release="2" source_lang_id="en-US" current_release="3" base_dir=".">
  <release seq="3">
    <messages>
      <if expr="pp_ifdef('hello')">
        <then>
          <message name="IDS_HELLO" use_name_for_id="true">
            Hello!
          </message>
        </then>
        <else>
          <message name="IDS_HELLO" use_name_for_id="true">
            Yellow!
          </message>
        </else>
      </if>
    </messages>
  </release>
</grit>'''
    pseudo_file = io.StringIO(input)
    root = grd_reader.Parse(pseudo_file, '.', defines={'hello': '1'})

    # Check if the ID is set to the name. In the past, there was a bug
    # that caused the ID to be a generated number.
    hello = root.GetNodeById('IDS_HELLO')
    self.assertTrue(hello.GetCliques()[0].GetId() == 'IDS_HELLO')

  def testPartInclusionAndCorrectSource(self):
    arbitrary_path_grd = '''\
        <grit-part>
          <message name="IDS_TEST5" desc="test5">test5</message>
        </grit-part>'''
    tmp_dir = util.TempDir({'arbitrary_path.grp': arbitrary_path_grd})
    arbitrary_path_grd_file = tmp_dir.GetPath('arbitrary_path.grp')
    top_grd = '''\
        <grit latest_public_release="2" current_release="3">
          <release seq="3">
            <messages>
              <message name="IDS_TEST" desc="test">
                test
              </message>
              <part file="sub.grp" />
              <part file="%s" />
            </messages>
          </release>
        </grit>''' % arbitrary_path_grd_file
    sub_grd = '''\
        <grit-part>
          <message name="IDS_TEST2" desc="test2">test2</message>
          <part file="subsub.grp" />
          <message name="IDS_TEST3" desc="test3">test3</message>
        </grit-part>'''
    subsub_grd = '''\
        <grit-part>
          <message name="IDS_TEST4" desc="test4">test4</message>
        </grit-part>'''
    expected_output = '''\
        <grit current_release="3" latest_public_release="2">
          <release seq="3">
            <messages>
              <message desc="test" name="IDS_TEST">
                test
              </message>
              <part file="sub.grp">
                <message desc="test2" name="IDS_TEST2">
                  test2
                </message>
                <part file="subsub.grp">
                  <message desc="test4" name="IDS_TEST4">
                    test4
                  </message>
                </part>
                <message desc="test3" name="IDS_TEST3">
                  test3
                </message>
              </part>
              <part file="%s">
                <message desc="test5" name="IDS_TEST5">
                  test5
                </message>
              </part>
            </messages>
          </release>
        </grit>''' % arbitrary_path_grd_file

    with util.TempDir({'sub.grp': sub_grd,
                       'subsub.grp': subsub_grd}) as tmp_sub_dir:
      output = grd_reader.Parse(io.StringIO(top_grd), tmp_sub_dir.GetPath())
      correct_sources = {
        'IDS_TEST': None,
        'IDS_TEST2': tmp_sub_dir.GetPath('sub.grp'),
        'IDS_TEST3': tmp_sub_dir.GetPath('sub.grp'),
        'IDS_TEST4': tmp_sub_dir.GetPath('subsub.grp'),
        'IDS_TEST5': arbitrary_path_grd_file,
      }

    for node in output.ActiveDescendants():
      with node:
        if isinstance(node, message.MessageNode):
          self.assertEqual(correct_sources[node.attrs.get('name')], node.source)
    self.assertEqual(expected_output.split(), output.FormatXml().split())
    tmp_dir.CleanUp()

  def testPartInclusionFailure(self):
    template = '''
      <grit latest_public_release="2" current_release="3">
        <outputs>
          %s
        </outputs>
      </grit>'''

    part_failures = [
        (exception.UnexpectedContent, '<part file="x">fnord</part>'),
        (exception.UnexpectedChild,
         '<part file="x"><output filename="x" type="y" /></part>'),
        (exception.FileNotFound, '<part file="yet_created_x" />'),
    ]
    for raises, data in part_failures:
      data = io.StringIO(template % data)
      self.assertRaises(raises, grd_reader.Parse, data, '.')

    gritpart_failures = [
        (exception.UnexpectedAttribute, '<grit-part file="xyz"></grit-part>'),
        (exception.MissingElement, '<output filename="x" type="y" />'),
    ]
    for raises, data in gritpart_failures:
      top_grd = io.StringIO(template % '<part file="bad.grp" />')
      with util.TempDir({'bad.grp': data}) as temp_dir:
        self.assertRaises(raises, grd_reader.Parse, top_grd, temp_dir.GetPath())

  def testEarlyEnoughPlatformSpecification(self):
    # This is a regression test for issue
    # https://code.google.com/p/grit-i18n/issues/detail?id=23
    grd_text = '''<?xml version="1.0" encoding="UTF-8"?>
      <grit latest_public_release="1" current_release="1">
        <release seq="1">
          <messages>
            <if expr="not pp_ifdef('use_titlecase')">
              <message name="IDS_XYZ">foo</message>
            </if>
            <!-- The assumption is that use_titlecase is never true for
                 this platform. When the platform isn't set to 'android'
                 early enough, we get a duplicate message name. -->
            <if expr="os == '%s'">
              <message name="IDS_XYZ">boo</message>
            </if>
          </messages>
        </release>
      </grit>''' % sys.platform
    with util.TempDir({}) as temp_dir:
      grd_reader.Parse(io.StringIO(grd_text),
                       temp_dir.GetPath(),
                       target_platform='android')

  def testSkipValidationChecks(self):
    input = '''<?xml version="1.0" encoding="UTF-8"?>
<grit latest_public_release="2" source_lang_id="en-US" current_release="3" base_dir=".">
  <release seq="3">
    <messages>
      <message name="IDS_MESSAGE1" desc="description 1">
        Message1
      </message>
      <message name="IDS_MESSAGE1" desc="description 1">
        Another message with the same |name|
      </message>
    </messages>
  </release>
</grit>'''
    pseudo_file = io.StringIO(input)

    # Test when |skip_validation_checks| is set, no error is thrown.
    grd_reader.Parse(pseudo_file, '.', skip_validation_checks=True)

    # Test when |skip_validation_checks| is set, an error is thrown.
    with self.assertRaises(DuplicateKey) as cm:
      pseudo_file = io.StringIO(input)
      grd_reader.Parse(pseudo_file, '.')
    self.assertTrue(
        str(cm.exception).startswith('A duplicate key attribute was found.'))


if __name__ == '__main__':
  unittest.main()
