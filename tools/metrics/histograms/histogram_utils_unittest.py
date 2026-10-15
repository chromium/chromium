# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import io
import pathlib
import tempfile
import unittest
from unittest import mock
import xml.etree.ElementTree as ET

import setup_modules  # pylint: disable=unused-import


import chromium_src.tools.metrics.histograms.histogram_utils as histogram_utils


class GetNamesTest(unittest.TestCase):
  def setUp(self):
    temp_dir = tempfile.TemporaryDirectory()
    self.addCleanup(temp_dir.cleanup)
    self.histograms = pathlib.Path(temp_dir.name) / 'histograms.xml'
    self.histograms.write_text(
      """\
<?xml version="1.0" encoding="utf-8"?>
<histogram-configuration>
  <histograms>
    <histogram name="Test.{TestVariants}" units="count" expires_after="never">
      <owner>test@chromium.org</owner>
      <summary>café {TestVariants}</summary>
    </histogram>
  </histograms>
</histogram-configuration>
""",
      encoding='utf-8',
    )
    self.variants = pathlib.Path(temp_dir.name) / 'variants.xml'
    self.variants.write_text(
      """\
<histogram-configuration>
  <variants name="TestVariants">
    <variant name="One" summary="one"/>
    <variant name="Two" summary="two"/>
  </variants>
</histogram-configuration>
""",
      encoding='utf-8',
    )

  def testFilenamesMergeBeforeExpandingVariants(self):
    self.assertEqual(
      histogram_utils.get_names([str(self.histograms), str(self.variants)]),
      {'Test.One', 'Test.Two'},
    )

  def testMixedInputsLeaveCallerStreamOpen(self):
    with self.histograms.open(encoding='utf-8') as stream:
      self.assertEqual(
        histogram_utils.get_names([stream, str(self.variants)]),
        {'Test.One', 'Test.Two'},
      )
      self.assertFalse(stream.closed)

  def testStringIOInputsRemainOpen(self):
    with io.StringIO(self.histograms.read_text(encoding='utf-8')) as stream:
      self.assertEqual(
        histogram_utils.get_names([stream, str(self.variants)]),
        {'Test.One', 'Test.Two'},
      )
      self.assertFalse(stream.closed)

  def testParseErrorClosesFilesOpenedFromFilenames(self):
    self.variants.write_text('<unclosed>', encoding='utf-8')
    opened_files = []
    real_open = open

    def track_open(*args, **kwargs):
      f = real_open(*args, **kwargs)
      opened_files.append(f)
      self.addCleanup(f.close)
      return f

    with mock.patch('builtins.open', side_effect=track_open):
      with self.assertRaises(ET.ParseError):
        histogram_utils.get_names([str(self.histograms), str(self.variants)])
    self.assertEqual(len(opened_files), 2)
    self.assertTrue(all(f.closed for f in opened_files))

  def testParseErrorLeavesCallerStreamOpen(self):
    with io.StringIO('<unclosed>') as stream:
      with self.assertRaises(ET.ParseError):
        histogram_utils.get_names([stream])
      self.assertFalse(stream.closed)


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
