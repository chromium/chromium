# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import unittest
import xml.dom.minidom

import extract_histograms

TEST_SUFFIX_OBSOLETION_XML_CONTENT = """
<histogram-configuration>
<histograms>
  <histogram name="Test.Test1" units="units">
    <owner>chrome-metrics-team@google.com</owner>
    <summary>
      Sample description.
    </summary>
  </histogram>
  <histogram name="Test.Test2" units="units">
    <owner>chrome-metrics-team@google.com</owner>
    <summary>
      Sample description.
    </summary>
  </histogram>
</histograms>
<histogram_suffixes_list>
  <histogram_suffixes name="NonObsoleteSuffix" separator="_">
    <suffix name="NonObsolete1" label="First non-obsolete suffix"/>
    <suffix name="NonObsolete2" label="Second non-obsolete suffix"/>
    <affected-histogram name="Test.Test1"/>
    <affected-histogram name="Test.Test2"/>
  </histogram_suffixes>

  <histogram_suffixes name="ObsoleteSuffixGroup" separator="_">
    <obsolete>This suffix group is obsolete</obsolete>
    <suffix name="ObsoleteSuffixGroup1" label="First obsolete suffix"/>
    <suffix name="ObsoleteSuffixGroup2" label="Second obsolete suffix"/>
    <affected-histogram name="Test.Test1"/>
  </histogram_suffixes>

  <histogram_suffixes name="ObsoleteSuffixNonObsoleteGroup" separator="_">
    <suffix name="ObsoleteSuffixNonObsoleteGroup1" label="Obsolete suffix">
      <obsolete>This suffix is obsolete</obsolete>
    </suffix>
    <suffix name="NonObsoleteSuffixNonObsoleteGroup2"
        label="Non obsolete suffix"/>
    <affected-histogram name="Test.Test2"/>
  </histogram_suffixes>

  <histogram_suffixes name="ObsoleteSuffixObsoleteGroup" separator="_">
    <obsolete>This suffix group is obsolete</obsolete>
    <suffix name="ObsoleteSuffixObsoleteGroup1" label="First obsolete suffix">
      <obsolete>This suffix is obsolete</obsolete>
    </suffix>
    <suffix name="NonObsoleteSuffixObsoleteGroup2"
        label="Second obsolete suffix"/>
    <affected-histogram name="Test.Test2"/>
  </histogram_suffixes>
</histogram_suffixes_list>
</histogram-configuration>
"""

TEST_BASE_HISTOGRAM_XML_CONTENT = """
<histogram-configuration>
<histograms>
  <histogram base="true" name="Test.Base" expires_after="2211-11-22"
      units="units">
    <owner>chrome-metrics-team@google.com</owner>
    <summary>
      Base histogram.
    </summary>
  </histogram>
  <histogram base="true" name="Test.Base.Obsolete" units="units">
    <owner>chrome-metrics-team@google.com</owner>
    <summary>
      Obsolete base histogram.
    </summary>
    <obsolete>
      The whole related set of histograms is obsolete!
    </obsolete>
  </histogram>
  <histogram base="false" name="Test.NotBase.Explicit" units="units" >
    <owner>chrome-metrics-team@google.com</owner>
    <summary>
      Not a base histogram: base attribute explicitly set to "false".
    </summary>
  </histogram>
  <histogram name="Test.NotBase.Implicit" expires_after="M100" units="units" >
    <owner>chrome-metrics-team@google.com</owner>
    <summary>
      Not a base histogram: no base attribute specified.
    </summary>
  </histogram>
</histograms>
<histogram_suffixes_list>
  <histogram_suffixes name="Suffixes" separator=".">
    <suffix base="true" name="BaseSuffix" label="A base suffix"/>
    <suffix name="NonBaseSuffix" label="A non-base suffix"/>
    <suffix name="ObsoleteSuffix" label="An obsolete suffix">
      <obsolete>This suffix is obsolete!</obsolete>
    </suffix>
    <affected-histogram name="Test.Base"/>
    <affected-histogram name="Test.Base.Obsolete"/>
  </histogram_suffixes>

  <histogram_suffixes name="SuffixesPlusPlus" separator=".">
    <suffix name="One" label="One suffix"/>
    <suffix name="Two" label="Another suffix"/>
    <affected-histogram name="Test.Base.BaseSuffix"/>
    <affected-histogram name="Test.Base.NonBaseSuffix"/>
  </histogram_suffixes>
</histogram_suffixes_list>
</histogram-configuration>
"""


class ExtractHistogramsTest(unittest.TestCase):

  def testSuffixObsoletion(self):
    histograms, had_errors = extract_histograms.ExtractHistogramsFromDom(
        xml.dom.minidom.parseString(TEST_SUFFIX_OBSOLETION_XML_CONTENT))
    self.assertFalse(had_errors)
    # Obsolete on suffixes doesn't affect the source histogram
    self.assertNotIn('obsolete', histograms['Test.Test1'])
    self.assertNotIn('obsolete', histograms['Test.Test2'])

    self.assertNotIn('obsolete', histograms['Test.Test1_NonObsolete1'])
    self.assertNotIn('obsolete', histograms['Test.Test1_NonObsolete2'])
    self.assertNotIn('obsolete', histograms['Test.Test2_NonObsolete1'])
    self.assertNotIn('obsolete', histograms['Test.Test2_NonObsolete2'])

    self.assertIn('obsolete', histograms['Test.Test1_ObsoleteSuffixGroup1'])
    self.assertIn('obsolete', histograms['Test.Test1_ObsoleteSuffixGroup2'])

    # Obsolete suffixes should apply to individual suffixes and not their group.
    self.assertIn('obsolete',
                  histograms['Test.Test2_ObsoleteSuffixNonObsoleteGroup1'])
    self.assertNotIn(
        'obsolete', histograms['Test.Test2_NonObsoleteSuffixNonObsoleteGroup2'])
    self.assertEqual(
        'This suffix is obsolete',
        histograms['Test.Test2_ObsoleteSuffixNonObsoleteGroup1']['obsolete'])

    # Obsolete suffix reasons should overwrite the suffix group's obsoletion
    # reason.
    self.assertIn('obsolete',
                  histograms['Test.Test2_ObsoleteSuffixObsoleteGroup1'])
    self.assertIn('obsolete',
                  histograms['Test.Test2_NonObsoleteSuffixObsoleteGroup2'])
    self.assertEqual(
        'This suffix is obsolete',
        histograms['Test.Test2_ObsoleteSuffixObsoleteGroup1']['obsolete'])
    self.assertEqual(
        'This suffix group is obsolete',
        histograms['Test.Test2_NonObsoleteSuffixObsoleteGroup2']['obsolete'])

  def testBaseHistograms(self):
    histograms, had_errors = extract_histograms.ExtractHistogramsFromDom(
        xml.dom.minidom.parseString(TEST_BASE_HISTOGRAM_XML_CONTENT))
    self.assertFalse(had_errors)
    # Base histograms are implicitly marked as obsolete.
    self.assertIn('obsolete', histograms['Test.Base'])
    self.assertIn('obsolete', histograms['Test.Base.Obsolete'])

    # Other histograms shouldn't be implicitly marked as obsolete.
    self.assertNotIn('obsolete', histograms['Test.NotBase.Explicit'])
    self.assertNotIn('obsolete', histograms['Test.NotBase.Implicit'])

    # Suffixes applied to base histograms shouldn't be marked as obsolete...
    self.assertNotIn('obsolete', histograms['Test.Base.NonBaseSuffix'])
    # ... unless the suffix is marked as obsolete,
    self.assertIn('obsolete', histograms['Test.Base.ObsoleteSuffix'])
    # ... or the suffix is a base suffix,
    self.assertIn('obsolete', histograms['Test.Base.BaseSuffix'])
    # ... or the base histogram is marked as obsolete,
    self.assertIn('obsolete', histograms['Test.Base.Obsolete.BaseSuffix'])
    self.assertIn('obsolete', histograms['Test.Base.Obsolete.NonBaseSuffix'])
    self.assertIn('obsolete', histograms['Test.Base.Obsolete.ObsoleteSuffix'])

    # It should be possible to have multiple levels of suffixes for base
    # histograms.
    self.assertNotIn('obsolete', histograms['Test.Base.BaseSuffix.One'])
    self.assertNotIn('obsolete', histograms['Test.Base.BaseSuffix.Two'])
    self.assertNotIn('obsolete', histograms['Test.Base.NonBaseSuffix.One'])
    self.assertNotIn('obsolete', histograms['Test.Base.NonBaseSuffix.Two'])

  def testExpiryFormat(self):
    chrome_histogram_pattern = """<histogram-configuration>

<histograms>

<histogram name="Histogram.Name"{} units="units" >
  <owner>SomeOne@google.com</owner>
  <summary>Summary</summary>
</histogram>

</histograms>

</histogram-configuration>
"""
    chrome_histogram_correct_expiry_date = chrome_histogram_pattern.format(
        ' expires_after="2211-11-22"')
    _, had_errors = extract_histograms.ExtractHistogramsFromDom(
        xml.dom.minidom.parseString(chrome_histogram_correct_expiry_date))
    self.assertFalse(had_errors)

    chrome_histogram_wrong_expiry_date_format = chrome_histogram_pattern.format(
        ' expires_after="2211/11/22"')
    _, had_errors = extract_histograms.ExtractHistogramsFromDom(
        xml.dom.minidom.parseString(chrome_histogram_wrong_expiry_date_format))
    self.assertTrue(had_errors)

    chrome_histogram_wrong_expiry_date_value = chrome_histogram_pattern.format(
        ' expires_after="2211-22-11"')
    _, had_errors = extract_histograms.ExtractHistogramsFromDom(
        xml.dom.minidom.parseString(chrome_histogram_wrong_expiry_date_value))
    self.assertTrue(had_errors)

    chrome_histogram_correct_expiry_milestone = chrome_histogram_pattern.format(
        ' expires_after="M22"')
    _, had_errors = extract_histograms.ExtractHistogramsFromDom(
        xml.dom.minidom.parseString(chrome_histogram_correct_expiry_milestone))
    self.assertFalse(had_errors)

    chrome_histogram_wrong_expiry_milestone = chrome_histogram_pattern.format(
        ' expires_after="22"')
    _, had_errors = extract_histograms.ExtractHistogramsFromDom(
        xml.dom.minidom.parseString(chrome_histogram_wrong_expiry_milestone))
    self.assertTrue(had_errors)

    chrome_histogram_wrong_expiry_milestone = chrome_histogram_pattern.format(
        ' expires_after="MM22"')
    _, had_errors = extract_histograms.ExtractHistogramsFromDom(
        xml.dom.minidom.parseString(chrome_histogram_wrong_expiry_milestone))
    self.assertTrue(had_errors)

    chrome_histogram_no_expiry = chrome_histogram_pattern.format('')
    _, had_errors = extract_histograms.ExtractHistogramsFromDom(
        xml.dom.minidom.parseString(chrome_histogram_no_expiry))
    self.assertFalse(had_errors)

  def testExpiryDateExtraction(self):
    chrome_histogram_pattern = """<histogram-configuration>

<histograms>

<histogram name="Histogram.Name"{} units="units">
  <owner>SomeOne@google.com</owner>
  <summary>Summary</summary>
</histogram>

</histograms>

</histogram-configuration>
"""
    date_str = '2211-11-22'
    chrome_histogram_correct_expiry_date = chrome_histogram_pattern.format(
        ' expires_after="{}"'.format(date_str))
    histograms, _ = extract_histograms.ExtractHistogramsFromDom(
        xml.dom.minidom.parseString(chrome_histogram_correct_expiry_date))
    histogram_content = histograms['Histogram.Name']
    self.assertIn('expires_after', histogram_content)
    self.assertEqual(date_str, histogram_content['expires_after'])

    milestone_str = 'M22'
    chrome_histogram_correct_expiry_milestone = chrome_histogram_pattern.format(
        ' expires_after="{}"'.format(milestone_str))
    histograms, _ = extract_histograms.ExtractHistogramsFromDom(
        xml.dom.minidom.parseString(chrome_histogram_correct_expiry_milestone))
    histogram_content = histograms['Histogram.Name']
    self.assertIn('expires_after', histogram_content)
    self.assertEqual(milestone_str, histogram_content['expires_after'])

    chrome_histogram_no_expiry = chrome_histogram_pattern.format('')
    histograms, _ = extract_histograms.ExtractHistogramsFromDom(
        xml.dom.minidom.parseString(chrome_histogram_no_expiry))
    histogram_content = histograms['Histogram.Name']
    self.assertNotIn('expires_after', histogram_content)

  def testMultiParagraphSummary(self):
    multiple_paragraph_pattern = xml.dom.minidom.parseString("""
<histogram-configuration>
<histograms>
  <histogram name="MultiParagraphTest.Test1" units="units">
    <owner>chrome-metrics-team@google.com</owner>
    <summary>
      Sample description
      Sample description.
    </summary>
  </histogram>

  <histogram name="MultiParagraphTest.Test2" units="units">
    <owner>chrome-metrics-team@google.com</owner>
    <summary>
      Multi-paragraph sample description UI&gt;Browser.
      Words.

      Still multi-paragraph sample description.

      <!--Comments are allowed.-->

      Here.
    </summary>
  </histogram>
</histograms>
</histogram-configuration>
""")
    histograms, _ = extract_histograms._ExtractHistogramsFromXmlTree(
        multiple_paragraph_pattern, {})
    self.assertEqual(histograms['MultiParagraphTest.Test1']['summary'],
                     'Sample description Sample description.')
    self.assertEqual(
        histograms['MultiParagraphTest.Test2']['summary'],
        'Multi-paragraph sample description UI>Browser. Words.\n\n'
        'Still multi-paragraph sample description.\n\nHere.')

  def testNewHistogramWithoutSummary(self):
    histogram_without_summary = xml.dom.minidom.parseString("""
<histogram-configuration>
<histograms>
 <histogram name="Test.Histogram" units="things">
   <owner>person@chromium.org</owner>
 </histogram>
</histograms>
</histogram-configuration>
""")
    _, have_errors = extract_histograms._ExtractHistogramsFromXmlTree(
        histogram_without_summary, {})
    self.assertTrue(have_errors)

  def testNewHistogramWithoutEnumOrUnit(self):
    histogram_without_enum_or_unit = xml.dom.minidom.parseString("""
<histogram-configuration>
<histograms>
 <histogram name="Test.Histogram">
  <owner>chrome-metrics-team@google.com</owner>
  <summary> This is a summary </summary>
 </histogram>
</histograms>
</histogram-configuration>
""")
    _, have_errors = extract_histograms._ExtractHistogramsFromXmlTree(
        histogram_without_enum_or_unit, {})
    self.assertTrue(have_errors)

  def testNewHistogramWithEnum(self):
    histogram_with_enum = xml.dom.minidom.parseString("""
<histogram-configuration>
<enums>
  <enum name="MyEnumType">
    <summary>This is an example enum type</summary>
    <int value="1" label="FIRST_VALUE">This is the first value.</int>
    <int value="2" label="SECOND_VALUE">This is the second value.</int>
  </enum>
</enums>

<histograms>
 <histogram name="Test.Histogram.Enum" enum="MyEnumType">
  <owner>chrome-metrics-team@google.com</owner>
  <summary> This is a summary </summary>
 </histogram>
</histograms>
</histogram-configuration>
""")
    _, have_errors = extract_histograms.ExtractHistogramsFromDom(
        histogram_with_enum)
    self.assertFalse(have_errors)

  def testNewHistogramWithUnits(self):
    histogram_with_units = xml.dom.minidom.parseString("""
<histogram-configuration>
<histograms>
 <histogram name="Test.Histogram" units="units">
  <owner>chrome-metrics-team@google.com</owner>
  <summary> This is a summary </summary>
 </histogram>
</histograms>
</histogram-configuration>
""")
    _, have_errors = extract_histograms._ExtractHistogramsFromXmlTree(
        histogram_with_units, {})
    self.assertFalse(have_errors)

  def testNewHistogramWithEmptyOwnerTag(self):
    histogram_with_empty_owner_tag = xml.dom.minidom.parseString("""
<histogram-configuration>
<histograms>
 <histogram name="Test.Histogram" units="things">
  <owner></owner>
  <summary> This is a summary </summary>
 </histogram>
</histograms>
</histogram-configuration>
""")
    _, have_errors = extract_histograms._ExtractHistogramsFromXmlTree(
        histogram_with_empty_owner_tag, {})
    self.assertTrue(have_errors)

  def testNewHistogramWithoutOwnerTag(self):
    histogram_without_owner_tag = xml.dom.minidom.parseString("""
<histogram-configuration>
<histograms>
 <histogram name="Test.Histogram" units="things">
  <summary> This is a summary </summary>
 </histogram>
</histograms>
</histogram-configuration>
""")
    _, have_errors = extract_histograms._ExtractHistogramsFromXmlTree(
        histogram_without_owner_tag, {})
    self.assertTrue(have_errors)

  def testNewHistogramWithCommaSeparatedOwners(self):
    histogram_with_comma_separated_owners = xml.dom.minidom.parseString("""
<histogram-configuration>
<histograms>
 <histogram name="Test.Histogram" units="things">
  <owner>cait@chromium.org, paul@chromium.org</owner>
  <summary> This is a summary </summary>
 </histogram>
</histograms>
</histogram-configuration>
""")
    _, have_errors = extract_histograms._ExtractHistogramsFromXmlTree(
        histogram_with_comma_separated_owners, {})
    self.assertTrue(have_errors)

  def testNewHistogramWithInvalidOwner(self):
    histogram_with_invalid_owner = xml.dom.minidom.parseString("""
<histogram-configuration>
<histograms>
 <histogram name="Test.Histogram" units="things">
  <owner>sarah</owner>
  <summary> This is a summary </summary>
 </histogram>
</histograms>
</histogram-configuration>
""")
    _, have_errors = extract_histograms._ExtractHistogramsFromXmlTree(
        histogram_with_invalid_owner, {})
    self.assertTrue(have_errors)

  def testNewHistogramWithOwnerPlaceHolder(self):
    histogram_with_owner_placeholder = xml.dom.minidom.parseString("""
<histogram-configuration>
<histograms>
 <histogram name="Test.Histogram" units="things">
  <owner> Please list the metric's owners. Add more owner tags as needed.
  </owner>
  <summary>
    <!-- Comments are fine -->
    This is a summary
    <!-- Comments are fine -->
  </summary>
 </histogram>
</histograms>
</histogram-configuration>
""")
    _, have_errors = extract_histograms._ExtractHistogramsFromXmlTree(
        histogram_with_owner_placeholder, {})
    self.assertFalse(have_errors)

  def testHistogramWithEscapeCharacters(self):
    histogram_with_owner_placeholder = xml.dom.minidom.parseString("""
<histogram-configuration>
<histograms>
 <histogram name="Test.Histogram" units="things">
  <owner> Please list the metric's owners. Add more owner tags as needed.
  </owner>
  <summary>This is a summary with &amp; and &quot; and &apos;</summary>
 </histogram>
</histograms>
</histogram-configuration>
""")
    hists, have_errors = extract_histograms._ExtractHistogramsFromXmlTree(
        histogram_with_owner_placeholder, {})
    self.assertFalse(have_errors)
    self.assertIn('Test.Histogram', hists)
    self.assertIn('summary', hists['Test.Histogram'])
    self.assertEqual('This is a summary with & and " and \'',
                     hists['Test.Histogram']['summary'])

  def testNewSuffixWithoutLabel(self):
    suffix_without_label = xml.dom.minidom.parseString("""
<histogram-configuration>
<histogram_suffixes_list>
  <histogram_suffixes name="Suffixes" separator=".">
    <suffix base="true" name="BaseSuffix"/>
  </histogram_suffixes>
</histogram_suffixes_list>
</histogram-configuration>
""")
    _, have_errors = extract_histograms.ExtractHistogramsFromDom(
        suffix_without_label)
    self.assertTrue(have_errors)

  def testNewSuffixWithLabel(self):
    suffix_with_label = xml.dom.minidom.parseString("""
<histogram-configuration>
<histogram_suffixes_list>
  <histogram_suffixes name="Suffixes" separator=".">
    <suffix base="true" name="BaseSuffix" label="Base"/>
  </histogram_suffixes>
</histogram_suffixes_list>
</histogram-configuration>
""")
    have_errors = extract_histograms. _UpdateHistogramsWithSuffixes(
        suffix_with_label, {})
    self.assertFalse(have_errors)

if __name__ == "__main__":
  logging.basicConfig(level=logging.ERROR + 1)
  unittest.main()
