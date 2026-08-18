# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import unittest.mock
import xml.dom.minidom

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
    variants_doc = xml.dom.minidom.parseString(variants_xml)
    names = histogram_utils.get_names_from_contents(
      contents.splitlines(), variants_doc
    )
    self.assertEqual(names, {'Test.V1', 'Test.V2'})

  def testFindFilesUsingVariants(self):
    content = """
<histogram-configuration>
<histograms>
  <histogram name="Test.{MockVariants}" enum="Boolean"/>
</histograms>
</histogram-configuration>
"""

    with unittest.mock.patch.object(
      histogram_utils, '_path_contents', return_value=content
    ) as mock_read:
      files = histogram_utils.find_files_using_variants(
        {'MockVariants'}, ['dummy_path.xml']
      )
      self.assertEqual(files, ['dummy_path.xml'])
      mock_read.assert_called_once_with('dummy_path.xml')


if __name__ == '__main__':
  unittest.main()
