# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import xml.dom.minidom

import expand_owners
import histogram_paths
import merge_xml


class MergeXmlTest(unittest.TestCase):

  def setUp(self):
    super().setUp()
    # Make assertMultiLineEqual() produce useful diffs.
    self.maxDiff = None

  def testMergeFiles(self):
    """Checks that the different XML files can merge successfully."""
    # Note: See the test files under src/tools/metrics/histograms/test_data.
    merged = merge_xml.PrettyPrintMergedFiles([
        histogram_paths.TEST_ENUMS_XML,  # Defines Enum_A and Enum_X.
        histogram_paths.TEST_ENUMS2_XML,  # Defines Enum_B.
        histogram_paths.TEST_HISTOGRAMS_XML,
        histogram_paths.TEST_SUFFIXES_XML,
    ])
    # If ukm.xml is not provided, there is no need to populate the
    # UkmEventNameHash enum.
    expected_merged_xml = """
<histogram-configuration>

<enums>

<enum name="Enum_A">
  <int value="0" label="Value0"/>
  <int value="1" label="Value1"/>
</enum>

<enum name="Enum_B">
  <int value="5" label="five"/>
  <int value="6" label="six"/>
</enum>

<enum name="Enum_X">
  <int value="0" label="Value0"/>
  <int value="1" label="Value1"/>
</enum>

<enum name="UkmEventNameHash">
  <summary>
    Placeholder enum. The values are UKM event name hashes truncated to 31 bits.
    This gets populated by the GetEnumsNodes function in merge_xml.py when
    producing the merged XML file.
  </summary>
</enum>

</enums>

<histograms>

<variants name="TestToken">
  <variant name="Variant1" summary="Label1"/>
  <variant name="Variant2" summary="Label2"/>
</variants>

<histogram name="Foo.Bar" units="xxxxxxxxxxxxxxxxxxyyyyyyyyyyyyyyyyyyyyyyzzzz"
    expires_after="M85">
  <owner>person@chromium.org</owner>
  <component>Component</component>
  <summary>Foo</summary>
</histogram>

<histogram name="Test.EnumHistogram" enum="TestEnum" expires_after="M81">
  <owner>uma@chromium.org</owner>
  <summary>A enum histogram.</summary>
</histogram>

<histogram name="Test.Histogram" units="microseconds" expires_after="M85">
  <owner>person@chromium.org</owner>
  <summary>Summary 2</summary>
</histogram>

<histogram name="Test.TokenHistogram{TestToken}" units="microseconds"
    expires_after="M85">
  <owner>person@chromium.org</owner>
  <summary>Summary 2</summary>
  <token key="TestToken" variants="TestToken"/>
</histogram>

</histograms>

<histogram_suffixes_list>

<histogram_suffixes name="Test.EnumHistogramSuffixes" separator="."
    ordering="prefix,2">
  <suffix name="TestEnumSuffix" label="The enum histogram_suffixes"/>
  <affected-histogram name="Test.EnumHistogram"/>
</histogram_suffixes>

<histogram_suffixes name="Test.HistogramSuffixes" separator=".">
  <suffix name="TestSuffix" label="A histogram_suffixes"/>
  <affected-histogram name="Test.Histogram"/>
</histogram_suffixes>

</histogram_suffixes_list>

</histogram-configuration>
"""
    self.assertMultiLineEqual(expected_merged_xml.strip(), merged.strip())

  def testMergeFiles_WithXmlEvents(self):
    """Checks that the UkmEventNameHash enum is populated correctly.

    If ukm.xml is provided, populate a list of ints to the UkmEventNameHash enum
    where each value is a truncated hash of the event name and each label is the
    corresponding event name, with obsolete label when applicable.
    """
    merged = merge_xml.PrettyPrintMergedFiles(histogram_paths.ALL_TEST_XMLS)
    expected_merged_xml = """
<histogram-configuration>

<enums>

<enum name="Enum_A">
  <int value="0" label="Value0"/>
  <int value="1" label="Value1"/>
</enum>

<enum name="Enum_B">
  <int value="5" label="five"/>
  <int value="6" label="six"/>
</enum>

<enum name="Enum_X">
  <int value="0" label="Value0"/>
  <int value="1" label="Value1"/>
</enum>

<enum name="UkmEventNameHash">
  <summary>
    Placeholder enum. The values are UKM event name hashes truncated to 31 bits.
    This gets populated by the GetEnumsNodes function in merge_xml.py when
    producing the merged XML file.
  </summary>
  <int value="151676257" label="AbusiveExperienceHeuristic.TestEvent1"/>
  <int value="898353372"
      label="AbusiveExperienceHeuristic.TestEvent2 (Obsolete)"/>
  <int value="1052089961" label="Autofill.TestEvent3"/>
  <int value="1758741469" label="FullyObsolete.TestEvent4 (Obsolete)"/>
</enum>

</enums>

<histograms>

<variants name="TestToken">
  <variant name="Variant1" summary="Label1"/>
  <variant name="Variant2" summary="Label2"/>
</variants>

<histogram name="Foo.Bar" units="xxxxxxxxxxxxxxxxxxyyyyyyyyyyyyyyyyyyyyyyzzzz"
    expires_after="M85">
  <owner>person@chromium.org</owner>
  <component>Component</component>
  <summary>Foo</summary>
</histogram>

<histogram name="Test.EnumHistogram" enum="TestEnum" expires_after="M81">
  <owner>uma@chromium.org</owner>
  <summary>A enum histogram.</summary>
</histogram>

<histogram name="Test.Histogram" units="microseconds" expires_after="M85">
  <owner>person@chromium.org</owner>
  <summary>Summary 2</summary>
</histogram>

<histogram name="Test.TokenHistogram{TestToken}" units="microseconds"
    expires_after="M85">
  <owner>person@chromium.org</owner>
  <summary>Summary 2</summary>
  <token key="TestToken" variants="TestToken"/>
</histogram>

</histograms>

<histogram_suffixes_list>

<histogram_suffixes name="Test.EnumHistogramSuffixes" separator="."
    ordering="prefix,2">
  <suffix name="TestEnumSuffix" label="The enum histogram_suffixes"/>
  <affected-histogram name="Test.EnumHistogram"/>
</histogram_suffixes>

<histogram_suffixes name="Test.HistogramSuffixes" separator=".">
  <suffix name="TestSuffix" label="A histogram_suffixes"/>
  <affected-histogram name="Test.Histogram"/>
</histogram_suffixes>

</histogram_suffixes_list>

</histogram-configuration>
"""
    self.assertMultiLineEqual(expected_merged_xml.strip(), merged.strip())


  def testMergeFiles_InvalidPrimaryOwner(self):
    histograms_without_valid_first_owner = xml.dom.minidom.parseString("""
<histogram-configuration>
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>culprit@evil.com</owner>
  <summary>I like coffee.</summary>
</histogram>

</histograms>
</histogram-configuration>
""")

    with self.assertRaisesRegex(
        expand_owners.Error,
        'The histogram Caffeination must have a valid primary owner, i.e. a '
        'Googler with an @google.com or @chromium.org email address. Please '
        'manually update the histogram with a valid primary owner.'):
      merge_xml.MergeTrees([histograms_without_valid_first_owner],
                           should_expand_owners=True)

  def testMergeFiles_WithComponentMetadata(self):
    merged = merge_xml.PrettyPrintMergedFiles(
        [histogram_paths.TEST_XML_WITH_COMPONENTS])
    expected_merged_xml = """
<histogram-configuration>

<enums/>

<histograms>

<histogram name="Test.Histogram" units="seconds" expires_after="M104">
  <owner>person@chromium.org</owner>
  <owner>team-alias@chromium.org</owner>
  <component>45678</component>
  <summary>Summary 2</summary>
</histogram>

<histogram name="Test.Histogram.WithComponent" enum="TestEnum"
    expires_after="M104">
  <owner>uma@chromium.org</owner>
  <owner>team-alias@chromium.org</owner>
  <component>First&gt;Component</component>
  <component>45678</component>
  <summary>A enum histogram.</summary>
</histogram>

</histograms>

<histogram_suffixes_list/>

</histogram-configuration>
"""
    self.assertMultiLineEqual(expected_merged_xml.strip(), merged.strip())


if __name__ == '__main__':
  unittest.main()
