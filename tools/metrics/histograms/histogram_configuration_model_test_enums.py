# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from parameterized import parameterized
import sys
import unittest

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import etree_util

import histogram_configuration_model

PRETTY_XML_RIGHT_ENUM_ORDER = """
<histogram-configuration>

<enums>

<enum name="Enum1">
  <int value="0" label="Label1"/>
  <int value="1" label="Label2"/>
</enum>

<enum name="Enum2">
  <summary>Summary text</summary>
  <int value="0" label="Label1"/>
  <int value="1" label="Label2"/>
</enum>

<enum name="Enum3">
  <obsolete>
    Obsolete text
  </obsolete>
  <summary>Summary text</summary>
  <int value="0" label="Label1">Int text</int>
  <int value="1" label="Label2"/>
</enum>

</enums>

</histogram-configuration>
""".strip()

PRETTY_XML = """
<histogram-configuration>

<enums>

<enum name="Enum1">
  <obsolete>
    Obsolete text
  </obsolete>
  <summary>Summary text</summary>
  <int value="0" label="Label1">Int text</int>
  <int value="1" label="Label2"/>
</enum>

</enums>

</histogram-configuration>
""".strip()

PRETTY_XML_COMMENTS = """
<histogram-configuration>

<enums>

<enum name="Enum1">
<!-- Comment 1 -->

  <obsolete>
    Obsolete text
  </obsolete>
  <summary>Summary text</summary>
  <int value="0" label="Label1">Int text</int>
  <int value="1" label="Label2"/>
<!-- Comment 2 -->

</enum>

<!-- Comment 3 -->

</enums>

<!-- Comment 4 -->

<!-- Comment 5 -->

</histogram-configuration>
""".strip()

XML_WRONG_ATTRIBUTE_ORDER = """
<histogram-configuration>

<enums>

<enum name="Enum1">
  <obsolete>
    Obsolete text
  </obsolete>
  <summary>Summary text</summary>
  <int label="Label1" value="0" >Int text</int>
  <int value="1" label="Label2"/>
</enum>

</enums>

</histogram-configuration>
""".strip()

XML_WRONG_INDENT = """
<histogram-configuration>

<enums>

  <enum name="Enum1">
    <obsolete>
      Obsolete text
    </obsolete>
    <summary>Summary text</summary>
      <int value="0" label="Label1">Int text</int>
      <int value="1" label="Label2"/>
  </enum>

</enums>

</histogram-configuration>
""".strip()

XML_WRONG_SINGLELINE = """
<histogram-configuration>

<enums>

<enum name="Enum1">
  <obsolete>Obsolete text</obsolete>
  <summary>
    Summary text
  </summary>
    <int value="0" label="Label1">Int text</int>
    <int value="1" label="Label2"/>
</enum>

</enums>

</histogram-configuration>
""".strip()

XML_WRONG_LINEBREAK = """
<histogram-configuration>
<enums>
<enum name="Enum1">
  <obsolete>
    Obsolete text
  </obsolete>
  <summary>Summary text</summary>

  <int value="0" label="Label1">Int text</int>
  <int value="1" label="Label2"/>
</enum>

</enums>


</histogram-configuration>
""".strip()

XML_WRONG_ENUM_ORDER = """
<histogram-configuration>

<enums>

<enum name="Enum2">
  <summary>Summary text</summary>
  <int value="0" label="Label1"/>
  <int value="1" label="Label2"/>
</enum>

<enum name="Enum3">
  <obsolete>
    Obsolete text
  </obsolete>
  <summary>Summary text</summary>
  <int value="0" label="Label1">Int text</int>
  <int value="1" label="Label2"/>
</enum>

<enum name="Enum1">
  <int value="0" label="Label1"/>
  <int value="1" label="Label2"/>
</enum>

</enums>

</histogram-configuration>
""".strip()

XML_WRONG_INT_ORDER = """
<histogram-configuration>

<enums>

<enum name="Enum1">
  <obsolete>
    Obsolete text
  </obsolete>
  <summary>Summary text</summary>
  <int value="1" label="Label2"/>
  <int value="0" label="Label1">Int text</int>
</enum>

</enums>

</histogram-configuration>
""".strip()

XML_WRONG_CHILDREN_ORDER = """
<histogram-configuration>

<enums>

<enum name="Enum1">
  <int value="0" label="Label1">Int text</int>
  <int value="1" label="Label2"/>
  <summary>Summary text</summary>
  <obsolete>
    Obsolete text
  </obsolete>
</enum>

</enums>

</histogram-configuration>
""".strip()

PRETTY_XML_WITH_COMMENTS = """
<histogram-configuration>

<enums>

<!-- Comment1 -->

<enum name="Enum1">
<!-- Comment2 -->

  <summary>Summary text</summary>
  <int value="0" label="Label1"/>
  <int value="1" label="Label2"/>
</enum>

<enum name="Enum2">
  <int value="0" label="Label1"/>
  <int value="1" label="Label2"/>
</enum>

</enums>

</histogram-configuration>
""".strip()

XML_WITH_COMMENTS_WRONG_INDENT_LINEBREAK = """
<histogram-configuration>

<enums>
  <!-- Comment1 -->

<enum name="Enum1">
<!-- Comment2 -->
  <summary>Summary text</summary>
  <int value="0" label="Label1"/>
  <int value="1" label="Label2"/>
</enum>

<enum name="Enum2">
  <int value="0" label="Label1"/>
  <int value="1" label="Label2"/>
</enum>

</enums>

</histogram-configuration>
""".strip()


PRETTY_XML_WITH_IFTTT_COMMENTS = """
<histogram-configuration>

<enums>

<!-- LINT.IfChange(Enum1) -->

<enum name="Enum1">
  <summary>Summary text</summary>
  <int value="0" label="Label1"/>
  <int value="1" label="Label2"/>
</enum>

<!-- LINT.ThenChange(//path/to/file.cpp:CppEnum1) -->

<enum name="Enum2">
<!-- LINT.IfChange(Enum2a) -->

  <int value="0" label="Label1"/>
  <int value="1" label="Label2"/>
<!-- LINT.ThenChange(//path/to/file.cpp:CppEnum2a) -->

<!-- LINT.IfChange(Enum2b) -->

  <int value="1000" label="Label3"/>
  <int value="1001" label="Label4"/>
<!-- LINT.ThenChange(//path/to/file.cpp:CppEnum2b) -->

</enum>

<!-- LINT.IfChange(Enum3) -->

<enum name="Enum3">
  <int value="0" label="Label1"/>
  <int value="1" label="Label2"/>
</enum>

<!-- LINT.ThenChange(//path/to/file.cpp:CppEnum3) -->

</enums>

</histogram-configuration>
""".strip()

PRETTY_XML_WITH_IFTTT_COMMENTS_MIDDLE = """
<histogram-configuration>

<enums>

<enum name="Enum1">
  <int value="0" label="Label1"/>
  <int value="1" label="Label2"/>
</enum>

<!-- LINT.IfChange(Enum2) -->

<enum name="Enum2">
  <int value="0" label="Label1"/>
  <int value="1" label="Label2"/>
</enum>

<!-- LINT.ThenChange(//path/to/file.cpp:CppEnum2) -->

<enum name="Enum3">
  <int value="0" label="Label1"/>
  <int value="1" label="Label2"/>
</enum>

</enums>

</histogram-configuration>
""".strip()

XML_IFTTT_COMMENTS_FOR_ENUM_VALUES = """
<histogram-configuration>
<enums>
<enum name="TestEnum">
  <!-- LINT.IfChange(//path/to/some/file) -->
  <int value="0" label="A"/>
  <!-- LINT.ThenChange(//path/to/other/file) -->
  <int value="2" label="C"/>
  <int value="1" label="B"/>
</enum>
</enums>
</histogram-configuration>
""".strip()

PRETTY_XML_IFTTT_COMMENTS_FOR_ENUM_VALUES = """
<histogram-configuration>

<enums>

<enum name="TestEnum">
<!-- LINT.IfChange(//path/to/some/file) -->

  <int value="0" label="A"/>
<!-- LINT.ThenChange(//path/to/other/file) -->

  <int value="1" label="B"/>
  <int value="2" label="C"/>
</enum>

</enums>

</histogram-configuration>
""".strip()

XML_IFTTT_COMMENTS_FOR_ENUM_VALUES_UNORDERED = """
<histogram-configuration>
<enums>
<enum name="TestEnum">
  <int value="1" label="B"/>

  <!-- LINT.IfChange(//path/to/some/file) -->
  <int value="0" label="A"/>
  <int value="2" label="C"/>
  <!-- LINT.ThenChange(//path/to/other/file) -->
</enum>
</enums>
</histogram-configuration>
""".strip()

PRETTY_XML_IFTTT_COMMENTS_FOR_ENUM_VALUES_UNORDERED = """
<histogram-configuration>

<enums>

<enum name="TestEnum">
<!-- LINT.IfChange(//path/to/some/file) -->

  <int value="0" label="A"/>
  <int value="2" label="C"/>
<!-- LINT.ThenChange(//path/to/other/file) -->

  <int value="1" label="B"/>
</enum>

</enums>

</histogram-configuration>
""".strip()

XML_IFTTT_INSIDE_ENUM_MULTIPLE_BLOCKS = """
<histogram-configuration>
<enums>
<enum name="num">
<!-- LINT.IfChange(B) -->
<int value="1" label="B"/>
<int value="3" label="D"/>
<!-- LINT.ThenChange(B) -->

<!-- LINT.IfChange(C) -->
<int value="5" label="F"/>
<int value="4" label="E"/>
<!-- LINT.ThenChange(C) -->

<!-- LINT.IfChange(A) -->
<int value="0" label="A"/>
<int value="2" label="C"/>
<!-- LINT.ThenChange(A) -->
</enum>
</enums>
</histogram-configuration>
""".strip()

PRETTY_IFTTT_INSIDE_ENUM_MULTIPLE_BLOCKS = """
<histogram-configuration>

<enums>

<enum name="num">
<!-- LINT.IfChange(A) -->

  <int value="0" label="A"/>
  <int value="2" label="C"/>
<!-- LINT.ThenChange(A) -->

<!-- LINT.IfChange(B) -->

  <int value="1" label="B"/>
  <int value="3" label="D"/>
<!-- LINT.ThenChange(B) -->

<!-- LINT.IfChange(C) -->

  <int value="4" label="E"/>
  <int value="5" label="F"/>
<!-- LINT.ThenChange(C) -->

</enum>

</enums>

</histogram-configuration>
""".strip()

XML_NESTED_IFTTT_OUTSIDE_ENUM = """
<histogram-configuration>
<enums>
<!-- LINT.IfChange(B) -->
<!-- LINT.IfChange(A) -->
<enum name="num">
<int value="1" label="B"/>
<int value="3" label="D"/>
<!-- LINT.ThenChange(A) -->

<int value="5" label="F"/>
<int value="4" label="E"/>
<!-- LINT.ThenChange(B) -->


<!-- LINT.IfChange(C) -->
<int value="0" label="A"/>
<int value="2" label="C"/>
<!-- LINT.ThenChange(C) -->
</enum>
</enums>
</histogram-configuration>
""".strip()

PRETTY_XML_NESTED_IFTTT_OUTSIDE_ENUM = """
<histogram-configuration>

<enums>

<!-- LINT.IfChange(B) -->

<!-- LINT.IfChange(A) -->

<enum name="num">
  <int value="1" label="B"/>
  <int value="3" label="D"/>
<!-- LINT.ThenChange(A) -->

  <int value="4" label="E"/>
  <int value="5" label="F"/>
<!-- LINT.ThenChange(B) -->

<!-- LINT.IfChange(C) -->

  <int value="0" label="A"/>
  <int value="2" label="C"/>
<!-- LINT.ThenChange(C) -->

</enum>

</enums>

</histogram-configuration>
""".strip()


PRETTY_XML_IFTTT_AND_REGULAR_COMMENTS = """
<histogram-configuration>

<enums>

<!-- NOTE: Important note -->

<!-- Regular comment -->

<!-- LINT.IfChange(Enum1) -->

<!-- Some other comment -->

<enum name="Enum1">
  <int value="0" label="Label1">Int text</int>
</enum>

<!-- Another comment -->

<!-- LINT.ThenChange(//path/to/file.cpp:CppEnum1) -->

<!-- Comment after lint -->

</enums>

</histogram-configuration>
""".strip()


class EnumXmlTest(unittest.TestCase):

  @parameterized.expand([
      # Test prettify already pretty XML to verify the pretty-printed version
      # is the same.
      ('AlreadyPrettyXml', PRETTY_XML, PRETTY_XML),
      ('PrettyXmlWithComments', PRETTY_XML_COMMENTS, PRETTY_XML_COMMENTS),
      ('AttributeOrder', XML_WRONG_ATTRIBUTE_ORDER, PRETTY_XML),
      ('Indent', XML_WRONG_INDENT, PRETTY_XML),
      ('SingleLine', XML_WRONG_SINGLELINE, PRETTY_XML),
      ('LineBreak', XML_WRONG_LINEBREAK, PRETTY_XML),
      # <int> tags of enums should be sorted by the integer value
      ('IntOrder', XML_WRONG_INT_ORDER, PRETTY_XML),
      # The children of enums should be sorted in the order of <obsolete>,
      # <summary> and <int>
      ('ChildrenOrder', XML_WRONG_CHILDREN_ORDER, PRETTY_XML),

      # Test prettify already pretty XML with right enum order to verify the
      # pretty-printed version is the same.
      ('AlreadyRightOrder', PRETTY_XML_RIGHT_ENUM_ORDER,
       PRETTY_XML_RIGHT_ENUM_ORDER),
      # Enums should be sorted in the order of their name attribute
      ('EnumOrder', XML_WRONG_ENUM_ORDER, PRETTY_XML_RIGHT_ENUM_ORDER),

      # Test prettify already pretty XML with comments to verify the
      # pretty-printed version is the same.
      ('AlreadyPrettyWithComments', PRETTY_XML_WITH_COMMENTS,
       PRETTY_XML_WITH_COMMENTS),
      ('CommentsIndentsLineBreak', XML_WITH_COMMENTS_WRONG_INDENT_LINEBREAK,
       PRETTY_XML_WITH_COMMENTS),

      # Tests that that LINT.IfChange / LINT.ThenChange comments are correctly
      # preserved in an already-pretty XML.
      ('AlreadyPrettyIfttt', PRETTY_XML_WITH_IFTTT_COMMENTS,
       PRETTY_XML_WITH_IFTTT_COMMENTS),
      ('AlreadyPrettyIftttMiddle', PRETTY_XML_WITH_IFTTT_COMMENTS_MIDDLE,
       PRETTY_XML_WITH_IFTTT_COMMENTS_MIDDLE),
      ('IFTTTCommentsForEnumValues', XML_IFTTT_COMMENTS_FOR_ENUM_VALUES,
       PRETTY_XML_IFTTT_COMMENTS_FOR_ENUM_VALUES),
      ('IFTTTCommentsForEnumValuesUnordered',
       XML_IFTTT_COMMENTS_FOR_ENUM_VALUES_UNORDERED,
       PRETTY_XML_IFTTT_COMMENTS_FOR_ENUM_VALUES_UNORDERED),
      ('IFTTTCommentsForEnumValuesMultipleBlocks',
       XML_IFTTT_INSIDE_ENUM_MULTIPLE_BLOCKS,
       PRETTY_IFTTT_INSIDE_ENUM_MULTIPLE_BLOCKS),
      ('NestedIFTTTCommentsOutsideEnum', XML_NESTED_IFTTT_OUTSIDE_ENUM,
       PRETTY_XML_NESTED_IFTTT_OUTSIDE_ENUM),
      ('IFTTTCommentsAndRegularCommentsOnTheSameLevel',
       PRETTY_XML_IFTTT_AND_REGULAR_COMMENTS,
       PRETTY_XML_IFTTT_AND_REGULAR_COMMENTS),
  ])
  def testPrettify(self, _, input_xml, expected_xml):
    result = histogram_configuration_model.PrettifyTree(
        etree_util.ParseXMLString(input_xml))
    self.assertMultiLineEqual(result.strip(), expected_xml)

  @parameterized.expand([
      # The "name" attribute of <enum> only allows alphanumeric characters
      # and punctuations "." and "_". It does not allow space.
      ('BadEnumNameIllegalPunctuation', PRETTY_XML, 'Enum1', 'Enum:1'),
      ('BadEnumNameWithSpace', PRETTY_XML, 'Enum1', 'Enum 1'),
      ('BadIntValueNegative', PRETTY_XML, '1', '-5'),
      ('BadIntValueNonNumeric', PRETTY_XML, '1', 'hello')
  ])
  def testRegex(self, _, pretty_input_xml, original_string, bad_string):
    BAD_XML = pretty_input_xml.replace(original_string, bad_string)
    with self.assertRaises(ValueError) as context:
      histogram_configuration_model.PrettifyTree(
          etree_util.ParseXMLString(BAD_XML))
    self.assertIn(bad_string, str(context.exception))
    self.assertIn('does not match regex', str(context.exception))


if __name__ == '__main__':
  unittest.main()
