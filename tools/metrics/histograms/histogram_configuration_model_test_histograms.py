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

XML_RIGHT_ORDER = """
<histogram-configuration>

<!-- Histogram definitions -->

<histograms>

<histogram name="hist.a" enum="enum1" expires_after="2019-11-02">
  <owner>owner1@chromium.org</owner>
  <owner>owner2@chromium.org</owner>
  <summary>Summary text</summary>
</histogram>

<histogram name="hist.b" expires_after="M85">
  <owner>owner1@chromium.org</owner>
  <owner>owner2@chromium.org</owner>
  <summary>Summary text</summary>
</histogram>

<histogram name="hist.c" enum="enum3" expires_after="never">
  <owner>owner1@chromium.org</owner>
  <owner>owner2@chromium.org</owner>
  <summary>Summary text</summary>
</histogram>

</histograms>

<histogram_suffixes_list>

<histogram_suffixes name="suffix1" separator="." ordering="prefix">
  <suffix base="true" name="suffix_name" label="label"/>
  <affected-histogram name="histogram1"/>
  <affected-histogram name="histogram2"/>
  <affected-histogram name="histogram3"/>
</histogram_suffixes>

<histogram_suffixes name="suffix2" separator="_" ordering="prefix,2">
  <suffix name="suffix_name" label="label"/>
  <affected-histogram name="histogram1"/>
  <affected-histogram name="histogram2"/>
  <affected-histogram name="histogram3"/>
</histogram_suffixes>

</histogram_suffixes_list>

</histogram-configuration>
""".strip()

PRETTY_XML = """
<histogram-configuration>

<!-- Histogram definitions -->

<histograms>

<histogram base="true" name="hist.a" expires_after="2019-11-02">
<!-- Comment in histogram -->

  <owner>owner1@chromium.org</owner>
  <owner>owner2@chromium.org</owner>
  <component>Component&gt;Subcomponent</component>
  <summary>Summary text</summary>
</histogram>

</histograms>

<histogram_suffixes_list>

<histogram_suffixes name="suffix1" separator="." ordering="prefix">
<!-- Comment in histogram_suffixes -->

  <suffix base="true" name="suffix_name" label="label"/>
  <suffix name="suffix_name" label="label"/>
  <affected-histogram name="histogram1"/>
</histogram_suffixes>

</histogram_suffixes_list>

</histogram-configuration>
""".strip()

XML_WRONG_ATTRIBUTE_ORDER = """
<histogram-configuration>

<!-- Histogram definitions -->

<histograms>

<histogram expires_after="2019-11-02" name="hist.a" base="true" >
<!-- Comment in histogram -->

  <owner>owner1@chromium.org</owner>
  <owner>owner2@chromium.org</owner>
  <component>Component&gt;Subcomponent</component>
  <summary>Summary text</summary>
</histogram>

</histograms>

<histogram_suffixes_list>

<histogram_suffixes name="suffix1" separator="." ordering="prefix">
<!-- Comment in histogram_suffixes -->

  <suffix name="suffix_name" base="true" label="label"/>
  <suffix label="label" name="suffix_name"/>
  <affected-histogram name="histogram1"/>
</histogram_suffixes>

</histogram_suffixes_list>

</histogram-configuration>
""".strip()

XML_MISSING_SEPARATOR = """
<histogram-configuration>

<!-- Histogram definitions -->

<histograms>

<histogram base="true" name="hist.a" expires_after="2019-11-02">
<!-- Comment in histogram -->

  <owner>owner1@chromium.org</owner>
  <owner>owner2@chromium.org</owner>
  <summary>Summary text</summary>
</histogram>

</histograms>

<histogram_suffixes_list>

<histogram_suffixes name="suffix1" ordering="prefix">
<!-- Comment in histogram_suffixes -->

  <suffix base="true" name="suffix_name" label="label"/>
  <suffix name="suffix_name" label="label"/>
  <affected-histogram name="histogram1"/>
</histogram_suffixes>

</histogram_suffixes_list>

</histogram-configuration>
""".strip()

XML_WRONG_INDENT = """
<histogram-configuration>

<!-- Histogram definitions -->

  <histograms>

  <histogram base="true" name="hist.a" expires_after="2019-11-02">
  <!-- Comment in histogram -->

      <owner>owner1@chromium.org</owner>
      <owner>owner2@chromium.org</owner>
      <component>Component&gt;Subcomponent</component>
    <summary>Summary text</summary>
  </histogram>

  </histograms>

  <histogram_suffixes_list>

  <histogram_suffixes name="suffix1" separator="." ordering="prefix">
  <!-- Comment in histogram_suffixes -->

      <suffix base="true" name="suffix_name" label="label"/>
      <suffix name="suffix_name" label="label"/>
      <affected-histogram name="histogram1"/>
  </histogram_suffixes>

  </histogram_suffixes_list>

</histogram-configuration>
""".strip()

XML_WRONG_SINGLELINE = """
<histogram-configuration>

<!-- Histogram definitions -->

<histograms>

<histogram base="true" name="hist.a" expires_after="2019-11-02">
<!-- Comment in histogram -->

  <owner>
    owner1@chromium.org
  </owner>
  <owner>owner2@chromium.org</owner>
  <component>
    Component&gt;Subcomponent
  </component>
  <summary>
    Summary text
  </summary>
</histogram>

</histograms>

<histogram_suffixes_list>

<histogram_suffixes name="suffix1" separator="." ordering="prefix">
<!-- Comment in histogram_suffixes -->

  <suffix base="true" name="suffix_name" label="label"/>
  <suffix name="suffix_name" label="label"/>
  <affected-histogram name="histogram1"/>
</histogram_suffixes>

</histogram_suffixes_list>

</histogram-configuration>
""".strip()

XML_WRONG_LINEBREAK = """
<histogram-configuration>

<!-- Histogram definitions -->
<histograms>

<histogram base="true" name="hist.a" expires_after="2019-11-02">
<!-- Comment in histogram -->
  <owner>owner1@chromium.org</owner>
  <owner>owner2@chromium.org</owner>
  <component>Component&gt;Subcomponent</component>
  <summary>Summary text</summary>

</histogram>
</histograms>

<histogram_suffixes_list>
<histogram_suffixes name="suffix1" separator="." ordering="prefix">
<!-- Comment in histogram_suffixes -->
  <suffix base="true" name="suffix_name" label="label"/>
  <suffix name="suffix_name" label="label"/>
  <affected-histogram name="histogram1"/>
</histogram_suffixes>
</histogram_suffixes_list>


</histogram-configuration>
""".strip()

XML_WRONG_CHILDREN_ORDER = """
<histogram-configuration>

<!-- Histogram definitions -->

<histograms>

<histogram base="true" name="hist.a" expires_after="2019-11-02">
<!-- Comment in histogram -->

  <owner>owner1@chromium.org</owner>
  <summary>Summary text</summary>
  <component>Component&gt;Subcomponent</component>
  <owner>owner2@chromium.org</owner>
</histogram>

</histograms>

<histogram_suffixes_list>

<histogram_suffixes name="suffix1" separator="." ordering="prefix">
<!-- Comment in histogram_suffixes -->

  <suffix base="true" name="suffix_name" label="label"/>
  <suffix name="suffix_name" label="label"/>
  <affected-histogram name="histogram1"/>
</histogram_suffixes>

</histogram_suffixes_list>

</histogram-configuration>
""".strip()

XML_WRONG_ORDER = """
<histogram-configuration>

<!-- Histogram definitions -->

<histograms>

<histogram name="hist.c" enum="enum3" expires_after="never">
  <owner>owner1@chromium.org</owner>
  <owner>owner2@chromium.org</owner>
  <summary>Summary text</summary>
</histogram>

<histogram name="hist.a" enum="enum1" expires_after="2019-11-02">
  <owner>owner1@chromium.org</owner>
  <owner>owner2@chromium.org</owner>
  <summary>Summary text</summary>
</histogram>

<histogram name="hist.b" expires_after="M85">
  <owner>owner1@chromium.org</owner>
  <owner>owner2@chromium.org</owner>
  <summary>Summary text</summary>
</histogram>

</histograms>

<histogram_suffixes_list>

<histogram_suffixes name="suffix2" separator="_" ordering="prefix,2">
  <suffix name="suffix_name" label="label"/>
  <affected-histogram name="histogram1"/>
  <affected-histogram name="histogram2"/>
  <affected-histogram name="histogram3"/>
</histogram_suffixes>

<histogram_suffixes name="suffix1" separator="." ordering="prefix">
  <suffix base="true" name="suffix_name" label="label"/>
  <affected-histogram name="histogram1"/>
  <affected-histogram name="histogram2"/>
  <affected-histogram name="histogram3"/>
</histogram_suffixes>

</histogram_suffixes_list>

</histogram-configuration>
""".strip()

PRETTY_XML_WITH_TOKEN = """
<histogram-configuration>

<!-- Histogram definitions -->

<histograms>

<variants name="OmniboxProviderVersion">
  <variant name="" summary="all versions"/>
  <variant name=".Provider2" summary="the second version"/>
</variants>

<histogram name="Omnibox{version}{content}.Time" units="ms"
    expires_after="2020-12-25">
  <owner>me@google.com</owner>
  <summary>
    The length of time taken by {version} of {content} provider's synchronous
    pass.
  </summary>
  <token key="version" variants="OmniboxProviderVersion"/>
  <token key="content">
    <variant name=".ExtensionApp" summary="ExtensionApp">
      <owner>you@google.com</owner>
    </variant>
    <variant name=".HistoryContents" summary="HistoryContents"/>
    <variant name=".HistoryQuick" summary="HistoryQuick"/>
  </token>
</histogram>

</histograms>

</histogram-configuration>
""".strip()

XML_WRONG_VARIANT_CHILDREN_ORDER = """
<histogram-configuration>

<!-- Histogram definitions -->

<histograms>

<variants name="OmniboxProviderVersion">
  <variant name="" summary="all versions"/>
  <variant name=".Provider2" summary="the second version"/>
</variants>

<histogram name="Omnibox{version}{content}.Time" units="ms"
    expires_after="2020-12-25">
  <owner>me@google.com</owner>
  <summary>
    The length of time taken by {version} of {content} provider's synchronous
    pass.
  </summary>
  <token key="version" variants="OmniboxProviderVersion"/>
  <token key="content">
    <variant name=".ExtensionApp" summary="ExtensionApp">
      <owner>you@google.com</owner>
    </variant>
    <variant name=".HistoryContents" summary="HistoryContents"/>
    <variant name=".HistoryQuick" summary="HistoryQuick"/>
  </token>
</histogram>

</histograms>

</histogram-configuration>
""".strip()

XML_WRONG_VARIANT_ORDER = """
<histogram-configuration>

<!-- Histogram definitions -->

<histograms>

<variants name="OmniboxProviderVersion">
  <variant name="" summary="all versions"/>
  <variant name=".Provider2" summary="the second version"/>
</variants>

<histogram name="Omnibox{version}{content}.Time" units="ms"
    expires_after="2020-12-25">
  <owner>me@google.com</owner>
  <summary>
    The length of time taken by {version} of {content} provider's synchronous
    pass.
  </summary>
  <token key="version" variants="OmniboxProviderVersion"/>
  <token key="content">
    <variant name=".ExtensionApp" summary="ExtensionApp">
      <owner>you@google.com</owner>
    </variant>
    <variant name=".HistoryQuick" summary="HistoryQuick"/>
    <variant name=".HistoryContents" summary="HistoryContents"/>
  </token>
</histogram>

</histograms>

</histogram-configuration>
""".strip()

XML_WRONG_HISTOGRAM_VARIANTS_ORDER = """
<histogram-configuration>

<!-- Histogram definitions -->

<histograms>

<histogram name="Omnibox{version}{content}.Time" units="ms"
    expires_after="2020-12-25">
  <owner>me@google.com</owner>
  <summary>
    The length of time taken by {version} of {content} provider's synchronous
    pass.
  </summary>
  <token key="version" variants="OmniboxProviderVersion"/>
  <token key="content">
    <variant name=".ExtensionApp" summary="ExtensionApp">
      <owner>you@google.com</owner>
    </variant>
    <variant name=".HistoryContents" summary="HistoryContents"/>
    <variant name=".HistoryQuick" summary="HistoryQuick"/>
  </token>
</histogram>

<variants name="OmniboxProviderVersion">
  <variant name="" summary="all versions"/>
  <variant name=".Provider2" summary="the second version"/>
</variants>

</histograms>

</histogram-configuration>
""".strip()


class HistogramXmlTest(unittest.TestCase):
  @parameterized.expand([
      # Test prettify already pretty XML to verify the pretty-printed version
      # is the same.
      ('AlreadyPrettyXml', PRETTY_XML, PRETTY_XML),
      ('AttributeOrder', XML_WRONG_ATTRIBUTE_ORDER, PRETTY_XML),
      ('Indent', XML_WRONG_INDENT, PRETTY_XML),
      ('SingleLine', XML_WRONG_SINGLELINE, PRETTY_XML),
      ('LineBreak', XML_WRONG_LINEBREAK, PRETTY_XML),

      # Test prettify already pretty XML with right order to verify the
      # pretty-printed version is the same.
      ('AlreadyPrettyXmlRightOrder', XML_RIGHT_ORDER, XML_RIGHT_ORDER),
      ('HistogramAndSuffixOrder', XML_WRONG_ORDER, XML_RIGHT_ORDER),
      ('ChildrenOrder', XML_WRONG_CHILDREN_ORDER, PRETTY_XML),
  ])
  def testPrettify(self, _, input_xml, expected_xml):
    result = histogram_configuration_model.PrettifyTree(
        etree_util.ParseXMLString(input_xml))
    self.maxDiff = None
    self.assertMultiLineEqual(expected_xml, result.strip())

  def testMissingRequiredAttribute(self):
    with self.assertRaises(Exception) as context:
      histogram_configuration_model.PrettifyTree(
          etree_util.ParseXMLString(XML_MISSING_SEPARATOR))
    self.assertIn('separator', str(context.exception))
    self.assertIn('Missing attribute', str(context.exception))

  @parameterized.expand([
      # The "base" attribute of <suffix> only allows
      # "true", "True", "false" or "False"
      ('BadSuffixBaseBoolean', XML_RIGHT_ORDER, 'true', 'yes'),

      # The "expires-after" attribute of <histogram> only allows:
      # Date given in the format YYYY-{M, MM}-{D, DD},
      # Milestone given in the format e.g. M81
      # or "never" or "Never"
      ('BadExpiresAfterDate', PRETTY_XML, '2019-11-02', 'Nov 2019'),
      ('BadExpiresAfterDateSeparator', PRETTY_XML, '2019-11-02', '2019/11/02'),
      ('BadExpiresAfterMilestone', XML_RIGHT_ORDER, 'M85', 'Milestone 85'),
      ('BadExpiresAfterNever', XML_RIGHT_ORDER, 'never', 'NEVER'),
      ('BadExpiresAfterOtherIllegalWords', XML_RIGHT_ORDER, 'never', 'hello'),

      # The "enum" attribute of <histogram> only allows alphanumeric characters
      # and punctuations "." and "_". It does not allow space.
      ('BadEnumNameIllegalPunctuation', XML_RIGHT_ORDER, 'enum1', 'enum:1'),
      ('BadEnumNameWithSpace', XML_RIGHT_ORDER, 'enum1', 'enum 1'),

      # The "ordering" attribute of <histogram_suffixes> only allow
      # "suffix", "prefix" or "prefix," followed by a non-negative integer
      ('BadOrderingIllegalPunctuation', XML_RIGHT_ORDER, 'prefix,2', 'prefix-2'
       ),
      ('BadOrderingNonNumber', XML_RIGHT_ORDER, 'prefix,2', 'prefix,two'),
  ])
  def testRegex(self, _, pretty_input_xml, original_string, bad_string):
    BAD_XML = pretty_input_xml.replace(original_string, bad_string)
    with self.assertRaises(ValueError) as context:
      histogram_configuration_model.PrettifyTree(
          etree_util.ParseXMLString(BAD_XML))
    self.assertIn(bad_string, str(context.exception))
    self.assertIn('does not match regex', str(context.exception))

  @parameterized.expand([
      # Test prettify already pretty XML to verify the pretty-printed version
      # is the same.
      ('AlreadyPrettyXml', PRETTY_XML_WITH_TOKEN, PRETTY_XML_WITH_TOKEN),
      ('ChildrenOrder', XML_WRONG_VARIANT_CHILDREN_ORDER,
       PRETTY_XML_WITH_TOKEN),
      ('VariantOrder', XML_WRONG_VARIANT_ORDER, PRETTY_XML_WITH_TOKEN),
      ('HistogramVariantsOrder', XML_WRONG_HISTOGRAM_VARIANTS_ORDER,
       PRETTY_XML_WITH_TOKEN),
  ])
  def testTokenPrettify(self, _, input_xml, expected_xml):
    result = histogram_configuration_model.PrettifyTree(
        etree_util.ParseXMLString(input_xml))
    self.maxDiff = None
    self.assertMultiLineEqual(expected_xml, result.strip())

  def testIndividualTagParsing_improvement(self):
    """Tests that <improvement> has the right format and can be parsed."""

    improvement_tag_good = '<improvement direction="HIGHER_IS_BETTER"/>'
    improvement_tag_bad = ' <improvement>HIGHER_IS_BETTER</improvement>'
    config = """
<histogram-configuration>

<histograms>

<histogram name="Histogram.With.ImprovementTag" expires_after="M100">
  <owner>owner1@chromium.org</owner>
  {improvement_tag}
  <summary>The improvement tag says higher value is good!</summary>
</histogram>

</histograms>

</histogram-configuration>"""

    config_good = config.format(improvement_tag=improvement_tag_good)
    config_bad = config.format(improvement_tag=improvement_tag_bad)

    result = histogram_configuration_model.PrettifyTree(
        etree_util.ParseXMLString(config_good))
    self.maxDiff = None
    self.assertMultiLineEqual(config_good.strip(), result.strip())

    with self.assertRaisesRegex(ValueError,
                                'direction "" does not match regex'):
      histogram_configuration_model.PrettifyTree(
          etree_util.ParseXMLString(config_bad))


if __name__ == '__main__':
  unittest.main()
