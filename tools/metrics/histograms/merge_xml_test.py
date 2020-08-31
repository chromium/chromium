# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import histogram_paths
import merge_xml


class MergeXmlTest(unittest.TestCase):

  def testMergeFiles(self):
    """Checks that enums.xml and histograms.xml can merge successfully."""
    merged = merge_xml.PrettyPrintMergedFiles([
        histogram_paths.TEST_ENUMS_XML, histogram_paths.TEST_HISTOGRAMS_XML,
        histogram_paths.TEST_SUFFIXES_XML
    ])
    # If ukm.xml is not provided, there is no need to populate the
    # UkmEventNameHash enum.
    expected_merged_xml = """
<histogram-configuration>

<enums>

<enum name="Enum1">
  <int value="0" label="Value0"/>
  <int value="1" label="Value1"/>
</enum>

<enum name="TestEnum">
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
  <summary>Foo</summary>
</histogram>

<histogram name="Test.EnumHistogram" enum="TestEnum" expires_after="M81">
  <obsolete>
    Obsolete message
  </obsolete>
  <owner>uma@chromium.org</owner>
  <summary>A enum histogram.</summary>
</histogram>

<histogram name="Test.Histogram" units="microseconds" expires_after="M85">
  <obsolete>
    Removed 6/2020.
  </obsolete>
  <owner>person@chromium.org</owner>
  <summary>Summary 2</summary>
</histogram>

<histogram name="Test.TokenHistogram{TestToken}" units="microseconds"
    expires_after="M85">
  <obsolete>
    Removed 6/2020.
  </obsolete>
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
    self.maxDiff = None
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

<enum name="Enum1">
  <int value="0" label="Value0"/>
  <int value="1" label="Value1"/>
</enum>

<enum name="TestEnum">
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
  <summary>Foo</summary>
</histogram>

<histogram name="Test.EnumHistogram" enum="TestEnum" expires_after="M81">
  <obsolete>
    Obsolete message
  </obsolete>
  <owner>uma@chromium.org</owner>
  <summary>A enum histogram.</summary>
</histogram>

<histogram name="Test.Histogram" units="microseconds" expires_after="M85">
  <obsolete>
    Removed 6/2020.
  </obsolete>
  <owner>person@chromium.org</owner>
  <summary>Summary 2</summary>
</histogram>

<histogram name="Test.TokenHistogram{TestToken}" units="microseconds"
    expires_after="M85">
  <obsolete>
    Removed 6/2020.
  </obsolete>
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
    self.maxDiff = None
    self.assertMultiLineEqual(expected_merged_xml.strip(), merged.strip())


if __name__ == '__main__':
  unittest.main()
