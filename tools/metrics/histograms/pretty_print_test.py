#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import pretty_print


ORIGINAL_XML = """
<!-- Top level Comment 1 -->
<!-- Top level Comment 2 -->
<histogram-configuration>
<histograms>
 <histogram name="Test.Histogram" units="us">
   <owner>person@chromium.org</owner>
   <summary>A long line that should be formatted in a way that does not result
     in extra whitespace between words.

        It has multiple paragraphs.
   </summary>
   Mixed content.
   <obsolete>
       Removed 1/2019.
   </obsolete>
 </histogram>
 <histogram name="Foo.Bar" units="xxxxxxxxxxxxxxxxxxyyyyyyyyyyyyyyyyyyyyyyzzzz">
  <summary>Foo</summary>
  <obsolete>Obsolete 1</obsolete>
  <obsolete>Obsolete 2</obsolete>
  <enums>This shouldn't be here</enums>
  <component>Component</component>
  <component>Other&gt;Component</component>
 </histogram>
</histograms>
<enums>This shouldn't be here</enums>
</histogram-configuration>
""".strip()


PRETTY_XML = """
<!-- Top level Comment 1 -->
<!-- Top level Comment 2 -->

<histogram-configuration>

<histograms>

<histogram name="Foo.Bar" units="xxxxxxxxxxxxxxxxxxyyyyyyyyyyyyyyyyyyyyyyzzzz">
  <obsolete>
    Obsolete 1
  </obsolete>
  <summary>Foo</summary>
  <component>Component</component>
  <component>Other&gt;Component</component>
</histogram>

<histogram name="Test.Histogram" units="microseconds">
  <obsolete>
    Removed 1/2019.
  </obsolete>
  <owner>person@chromium.org</owner>
  <summary>
    A long line that should be formatted in a way that does not result in extra
    whitespace between words.

    It has multiple paragraphs.
  </summary>
  Mixed content.
</histogram>

</histograms>

</histogram-configuration>
""".strip()


class PrettyPrintHistogramsXmlTest(unittest.TestCase):

  def testPrettyPrinting(self):
    result = pretty_print.PrettyPrintHistograms(ORIGINAL_XML)
    self.maxDiff = None
    self.assertMultiLineEqual(PRETTY_XML, result.strip())

if __name__ == '__main__':
  unittest.main()
