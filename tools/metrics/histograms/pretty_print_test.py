#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import pretty_print


ORIGINAL_XML = """
<histogram-configuration>
<histograms>
 <histogram name="Test.Histogram" units="things">
   <owner>person@chromium.org</owner>
   <summary>A long line that should be formatted in a way that does not result
     in extra whitespace between words.
   </summary>
 </histogram>
</histograms>
</histogram-configuration>
""".strip()


PRETTY_XML = """
<histogram-configuration>

<histograms>

<histogram name="Test.Histogram" units="things">
  <owner>person@chromium.org</owner>
  <summary>
    A long line that should be formatted in a way that does not result in extra
    whitespace between words.
  </summary>
</histogram>

</histograms>

</histogram-configuration>
""".strip()


class PrettyPrintHistogramsXmlTest(unittest.TestCase):

  def testWhitespaceWrapping(self):
    result = pretty_print.PrettyPrintHistograms(ORIGINAL_XML)
    self.maxDiff = None
    self.assertMultiLineEqual(PRETTY_XML, result.strip())


if __name__ == '__main__':
  unittest.main()
