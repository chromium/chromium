#!/usr/bin/env python
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import pretty_print


ORIGINAL_XML = """
<!-- Top level Comment 1 -->
<!-- Top level Comment 2 -->
<histogram-configuration>
<!-- Second level Comment 1 -->
<histograms>
 <histogram name="Test.Histogram" units="us">
   <owner>person@chromium.org</owner>
   <summary>A long line that should be formatted in a way that does not result
     in extra whitespace between words.

        It has multiple paragraphs.
   </summary>
 </histogram>

 <histogram name="Foo.Bar" units="xxxxxxxxxxxxxxxxxxyyyyyyyyyyyyyyyyyyyyyyzzzz">
  <summary>Foo</summary>
  <enums>This shouldn't be here</enums>
 </histogram>

 <histogram_suffixes name="Test.HistogramSuffixes" separator=".">
  <suffix name="TestSuffix" label="A misplaced histogram_suffixes"/>
  <affected-histogram name="Test.Histogram"/>
</histogram_suffixes>

</histograms>

<histogram_suffixes_list>

<histogram name="Test.MisplacedHistogram" units="us">
   <owner>person@chromium.org</owner>
   Misplaced content.
   <summary>A misplaced histogram
   </summary>
 </histogram>

</histogram_suffixes_list>

<enums>This shouldn't be here</enums>
</histogram-configuration>
""".strip()

PRETTY_XML = """
<!-- Top level Comment 1 -->
<!-- Top level Comment 2 -->

<histogram-configuration>

<!-- Second level Comment 1 -->

<histograms>

<histogram name="Foo.Bar" units="xxxxxxxxxxxxxxxxxxyyyyyyyyyyyyyyyyyyyyyyzzzz">
  <summary>Foo</summary>
</histogram>

<histogram name="Test.Histogram" units="microseconds">
  <owner>person@chromium.org</owner>
  <summary>
    A long line that should be formatted in a way that does not result in extra
    whitespace between words.

    It has multiple paragraphs.
  </summary>
</histogram>

<histogram name="Test.MisplacedHistogram" units="microseconds">
  <owner>person@chromium.org</owner>
  <summary>A misplaced histogram</summary>
</histogram>

</histograms>

<histogram_suffixes_list>

<histogram_suffixes name="Test.HistogramSuffixes" separator=".">
  <suffix name="TestSuffix" label="A misplaced histogram_suffixes"/>
  <affected-histogram name="Test.Histogram"/>
</histogram_suffixes>

</histogram_suffixes_list>

</histogram-configuration>
""".strip()


class PrettyPrintHistogramsXmlTest(unittest.TestCase):

  def testPrettyPrinting(self):
    result = pretty_print.PrettyPrintHistograms(ORIGINAL_XML)
    self.maxDiff = None
    self.assertMultiLineEqual(PRETTY_XML, result.strip())

if __name__ == '__main__':
  unittest.main()
