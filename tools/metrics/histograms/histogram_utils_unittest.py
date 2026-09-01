# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from unittest import mock
import xml.etree.ElementTree as ET

import setup_modules  # pylint: disable=unused-import

import chromium_src.tools.metrics.histograms.histogram_utils as histogram_utils


class HistogramUtilsTest(unittest.TestCase):
  def testGetModifiedVariantsBlocks_Added(self):
    old_content = """
<histogram-configuration>
<variants name="MockVariants">
  <variant name="V1"/>
</variants>
</histogram-configuration>
"""
    new_content = """
<histogram-configuration>
<variants name="MockVariants">
  <variant name="V1"/>
</variants>
<variants name="NewVariants">
  <variant name="V2"/>
</variants>
</histogram-configuration>
"""
    modified = histogram_utils.get_modified_variants_blocks(
      old_content, new_content
    )
    self.assertEqual(modified, {'NewVariants'})

  def testGetModifiedVariantsBlocks_Modified(self):
    old_content = """
<histogram-configuration>
<variants name="MockVariants">
  <variant name="V1"/>
</variants>
</histogram-configuration>
"""
    new_content = """
<histogram-configuration>
<variants name="MockVariants">
  <variant name="V1"/>
  <variant name="V2"/>
</variants>
</histogram-configuration>
"""
    modified = histogram_utils.get_modified_variants_blocks(
      old_content, new_content
    )
    self.assertEqual(modified, {'MockVariants'})

  def testGetModifiedVariantsBlocks_MetadataModified(self):
    old_content = """
<histogram-configuration>
<variants name="MockVariants">
  <variant name="V1" summary="Old summary"/>
</variants>
</histogram-configuration>
"""
    new_content = """
<histogram-configuration>
<variants name="MockVariants">
  <variant name="V1" summary="New summary"/>
</variants>
</histogram-configuration>
"""
    modified = histogram_utils.get_modified_variants_blocks(
      old_content, new_content
    )
    self.assertEqual(modified, {'MockVariants'})

  def testGetModifiedVariantsBlocks_Removed(self):
    old_content = """
<histogram-configuration>
<variants name="MockVariants">
  <variant name="V1"/>
</variants>
<variants name="RemovedVariants">
  <variant name="V2"/>
</variants>
</histogram-configuration>
"""
    new_content = """
<histogram-configuration>
<variants name="MockVariants">
  <variant name="V1"/>
</variants>
</histogram-configuration>
"""
    modified = histogram_utils.get_modified_variants_blocks(
      old_content, new_content
    )
    self.assertEqual(modified, {'RemovedVariants'})

  def testGetNamesFromContents(self):
    contents = """
<histogram-configuration>
<histograms>
  <histogram name="Test.{MockVariants}" enum="Boolean"/>
</histograms>
</histogram-configuration>
"""
    variants_xml = """
<histogram-configuration>
<variants name="MockVariants">
  <variant name="V1"/>
  <variant name="V2"/>
</variants>
</histogram-configuration>
"""
    variants_doc = ET.fromstring(variants_xml)
    names = histogram_utils.get_names_from_contents(
      contents.splitlines(), variants_doc
    )
    self.assertEqual(names, {'Test.V1', 'Test.V2'})

  def testGetNamesUsingVariantsRemovesStaleAffectedHistograms(self):
    contents = """
<histogram-configuration>
<histograms>
  <histogram name="Test.{MockVariants}" units="count" expires_after="M200">
    <owner>owner@chromium.org</owner>
    <summary>Records the test value.</summary>
    <token key="MockVariants" variants="MockVariants"/>
  </histogram>
  <histogram name="Unrelated.Histogram" units="count" expires_after="M200">
    <owner>owner@chromium.org</owner>
    <summary>Records the unrelated value.</summary>
  </histogram>
</histograms>
<histogram_suffixes_list>
  <histogram_suffixes name="Suffixes" separator=".">
    <suffix name="Suffix" label="A suffix"/>
    <affected-histogram name="Unrelated.Histogram"/>
  </histogram_suffixes>
</histogram_suffixes_list>
</histogram-configuration>
"""
    variants_xml = """
<histogram-configuration>
<variants name="MockVariants">
  <variant name="V1"/>
</variants>
</histogram-configuration>
"""
    variants_doc = ET.fromstring(variants_xml)

    # The mock verifies that filtering out the unrelated histogram also removes
    # its suffix reference instead of logging a missing-histogram error.
    with mock.patch.object(
      histogram_utils.extract_histograms.logging, 'error'
    ) as mock_log_error:
      names = histogram_utils.get_names_using_variants_from_contents(
        contents.splitlines(), variants_doc, {'MockVariants'}
      )

    self.assertEqual(names, {'Test.V1'})
    mock_log_error.assert_not_called()

  def testGetNamesUsingVariantsIgnoresInlineTokenKey(self):
    contents = """
<histogram-configuration>
<histograms>
  <histogram name="Test.{MockVariants}" units="count" expires_after="M200">
    <owner>owner@chromium.org</owner>
    <summary>Records the test value.</summary>
    <token key="MockVariants">
      <variant name="Inline"/>
    </token>
  </histogram>
</histograms>
</histogram-configuration>
"""
    variants_xml = """
<histogram-configuration>
<variants name="MockVariants">
  <variant name="Global"/>
</variants>
</histogram-configuration>
"""
    variants_doc = ET.fromstring(variants_xml)

    all_names = histogram_utils.get_names_from_contents(
      contents.splitlines(), variants_doc
    )
    names = histogram_utils.get_names_using_variants_from_contents(
      contents.splitlines(), variants_doc, {'MockVariants'}
    )

    # The explicit token supplies the inline value, so normal expansion uses
    # Test.Inline. It does not make this histogram a user of the same-named
    # global MockVariants block.
    self.assertEqual(all_names, {'Test.Inline'})
    self.assertEqual(names, set())

  def testFindFilesUsingVariants(self):
    content = """
<histogram-configuration>
<histograms>
  <histogram name="Test.{MockVariants}" enum="Boolean"/>
</histograms>
</histogram-configuration>
"""

    with mock.patch.object(
      histogram_utils, '_path_contents', return_value=content
    ) as mock_read:
      files = histogram_utils.find_files_using_variants(
        {'MockVariants'}, ['dummy_path.xml']
      )
      self.assertEqual(files, ['dummy_path.xml'])
      mock_read.assert_called_once_with('dummy_path.xml')


if __name__ == '__main__':
  unittest.main()
