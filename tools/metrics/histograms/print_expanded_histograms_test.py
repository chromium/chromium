#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from contextlib import redirect_stdout
import io
import os
import sys
import tempfile
import unittest
from unittest import mock

import print_expanded_histograms

TEST_HISTOGRAMS_XML = """
<histogram-configuration>
<histograms>
<histogram name="Test.Histogram" units="things" expires_after="2099-01-01">
  <owner>test@chromium.org</owner>
  <summary>A test histogram.</summary>
</histogram>
</histograms>
</histogram-configuration>
"""

TEST_SUFFIXES_XML = """
<histogram-configuration>
<histogram_suffixes_list>
  <histogram_suffixes name="TestSuffixes" separator=".">
    <suffix name="Foo" label="Foo suffix"/>
    <suffix name="Bar" label="Bar suffix"/>
    <affected-histogram name="Test.Histogram"/>
  </histogram_suffixes>
</histogram_suffixes_list>
</histogram-configuration>
"""

TEST_PATTERNED_HISTOGRAMS_XML = """
<histogram-configuration>
<histograms>
<histogram name="Test.PatternedHist.{Variant}" units="things"
    expires_after="2099-01-01">
  <owner>test@chromium.org</owner>
  <summary>A test patterned histogram with a {Variant}.</summary>
  <token key="Variant" variants="TestVariants"/>
</histogram>
</histograms>
</histogram-configuration>
"""

TEST_VARIANTS_XML = """
<histogram-configuration>
<variants name="TestVariants">
  <variant name="Variant1" summary="variant 1"/>
  <variant name="Variant2" summary="variant 2"/>
</variants>
</histogram-configuration>
"""


class PrintExpandedHistogramsTest(unittest.TestCase):
  """Smoke tests for print_expanded_histograms.py.

  These tests run the script's main() function, using mock input files and
  command-line arguments, check for a successful exit code and perform
  validation of the output to ensure that histograms are expanded correctly.
  """

  def _CreateTempFile(self, file_path, file_contents):
    xml_path = os.path.join(self.temp_dir.name, file_path)
    with open(xml_path, 'w') as f:
      f.write(file_contents)

    return xml_path

  def setUp(self):
    self.temp_dir = tempfile.TemporaryDirectory()

    # Create temporary test files
    self.histograms_xml_path = self._CreateTempFile('histograms.xml',
                                                    TEST_HISTOGRAMS_XML)
    self.suffixes_xml_path = self._CreateTempFile('suffixes.xml',
                                                  TEST_SUFFIXES_XML)
    self.patterned_histograms_xml_path = self._CreateTempFile(
        'patterned_histograms.xml', TEST_PATTERNED_HISTOGRAMS_XML)
    self.variants_xml_path = self._CreateTempFile('variants.xml',
                                                  TEST_VARIANTS_XML)

  def tearDown(self):
    self.temp_dir.cleanup()

  def _PatchAllXmls(self):
    return mock.patch('histogram_paths.ALL_XMLS', [
        self.histograms_xml_path, self.suffixes_xml_path,
        self.patterned_histograms_xml_path, self.variants_xml_path
    ])

  def testSmoke(self):
    mock_stdout = io.StringIO()
    with redirect_stdout(mock_stdout), self._PatchAllXmls():
      return_code = print_expanded_histograms.main([])
    self.assertEqual(0, return_code)
    output = mock_stdout.getvalue()
    self.assertIn('<histogram name="Test.Histogram.Foo"', output)
    self.assertIn('<histogram name="Test.Histogram.Bar"', output)
    self.assertIn('<histogram name="Test.PatternedHist.Variant1"', output)
    self.assertIn('<histogram name="Test.PatternedHist.Variant2"', output)

  def testNamesOnly(self):
    mock_stdout = io.StringIO()
    with redirect_stdout(mock_stdout), self._PatchAllXmls():
      return_code = print_expanded_histograms.main(['--print-names-only'])
    self.assertEqual(0, return_code)
    output = mock_stdout.getvalue()
    self.assertIn('Test.Histogram.Foo', output)
    self.assertIn('Test.Histogram.Bar', output)
    self.assertIn('Test.PatternedHist.Variant1', output)
    self.assertIn('Test.PatternedHist.Variant2', output)
    self.assertNotIn('<histogram name="Test.Histogram.Foo"', output)


if __name__ == '__main__':
  unittest.main()
