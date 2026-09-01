# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pathlib
import unittest
from unittest import mock
import xml.etree.ElementTree as ET

import setup_modules  # pylint: disable=unused-import

import chromium_src.tools.metrics.common.path_util as path_util
import chromium_src.tools.metrics.histograms.histogram_validation as histogram_validation


class HistogramValidationTest(unittest.TestCase):
  def testCheckBooleansAreEnums_Failure(self):
    target_path = str(
      pathlib.Path(
        'tools',
        'metrics',
        'histograms',
        'metadata',
        'uma',
        'histograms.xml',
      )
    )
    affected_files = [
      histogram_validation.AffectedFileForLineCheck(
        path=target_path,
        changed_lines=[
          (10, '<histogram name="Foo" units="Boolean">'),
          (12, '<histogram name="Bar" units="boolean">'),
        ],
      )
    ]
    errors = histogram_validation.check_booleans_are_enums(affected_files)
    self.assertEqual(
      errors,
      [
        (
          target_path,
          10,
          '<histogram name="Foo" units="Boolean">',
        ),
        (
          target_path,
          12,
          '<histogram name="Bar" units="boolean">',
        ),
      ],
    )

  def testCheckBooleansAreEnums_Pass(self):
    target_path = str(
      pathlib.Path(
        'tools',
        'metrics',
        'histograms',
        'metadata',
        'uma',
        'histograms.xml',
      )
    )
    affected_files = [
      histogram_validation.AffectedFileForLineCheck(
        path=target_path,
        changed_lines=[
          (10, '<histogram name="Foo" enum="Boolean">'),
          (12, '<histogram name="Bar" units="ms">'),
        ],
      )
    ]
    errors = histogram_validation.check_booleans_are_enums(affected_files)
    self.assertEqual(errors, [])

  def testCheckRemovedSegmentationHistograms_Failure(self):
    removed = {'Segmentation.Foo', 'Bar'}
    segmentation = {'Segmentation.Foo', 'Segmentation.Bar'}
    removed_seg = histogram_validation.check_removed_segmentation_histograms(
      removed, segmentation
    )
    self.assertEqual(removed_seg, {'Segmentation.Foo'})

  def testCheckRemovedSegmentationHistograms_Pass(self):
    removed = {'Foo', 'Bar'}
    segmentation = {'Segmentation.Foo', 'Segmentation.Bar'}
    removed_seg = histogram_validation.check_removed_segmentation_histograms(
      removed, segmentation
    )
    self.assertEqual(removed_seg, set())

  def testCheckIfIntroducedTooManyHistograms_Failure(self):
    added = {'H1', 'H2', 'H3'}
    self.assertTrue(
      histogram_validation.check_if_introduced_too_many_histograms(
        added, threshold=2
      )
    )

  def testCheckIfIntroducedTooManyHistograms_Pass(self):
    added = {'H1', 'H2'}
    self.assertFalse(
      histogram_validation.check_if_introduced_too_many_histograms(
        added, threshold=2
      )
    )

  def testGetFilesToCheck_NoChanges(self):
    affected_files = []

    def read_file_fn(_path):
      return '<histogram-configuration></histogram-configuration>'

    variants_path = str(
      pathlib.Path('tools', 'metrics', 'histograms', 'variants.xml')
    )
    histograms_path = str(
      pathlib.Path(
        'tools',
        'metrics',
        'histograms',
        'metadata',
        'uma',
        'histograms.xml',
      )
    )
    result = histogram_validation.get_files_to_check(
      affected_files,
      read_file_fn,
      variants_paths=[variants_path],
      histograms_paths=[histograms_path],
    )
    self.assertEqual(result.files_to_check, [])

  def testGetFilesToCheck_HistogramModified(self):
    target_path = str(
      (
        path_util.CHROMIUM_SRC_PATH
        / 'tools'
        / 'metrics'
        / 'histograms'
        / 'metadata'
        / 'uma'
        / 'histograms.xml'
      ).resolve()
    )
    affected_files = [
      histogram_validation.HistogramFileState(
        path=target_path,
        old_contents=[
          '<histogram-configuration><histograms><histogram'
          ' name="Foo"/></histograms></histogram-configuration>'
        ],
        new_contents=[
          '<histogram-configuration><histograms><histogram'
          ' name="Foo"/><histogram'
          ' name="Bar"/></histograms></histogram-configuration>'
        ],
        action='M',
      )
    ]

    def read_file_fn(_path):
      return '<histogram-configuration></histogram-configuration>'

    variants_path = str(
      (
        path_util.CHROMIUM_SRC_PATH
        / 'tools'
        / 'metrics'
        / 'histograms'
        / 'variants.xml'
      ).resolve()
    )
    result = histogram_validation.get_files_to_check(
      affected_files,
      read_file_fn,
      variants_paths=[variants_path],
      histograms_paths=[target_path],
    )
    self.assertEqual(len(result.files_to_check), 1)
    self.assertEqual(
      pathlib.Path(result.files_to_check[0].path), pathlib.Path(target_path)
    )

  def testGetFilesToCheck_DuplicatesInAffectedFiles(self):
    target_path = str(
      (
        path_util.CHROMIUM_SRC_PATH
        / 'tools'
        / 'metrics'
        / 'histograms'
        / 'metadata'
        / 'uma'
        / 'histograms.xml'
      ).resolve()
    )
    affected_files = [
      histogram_validation.HistogramFileState(
        path=target_path,
        old_contents=['<histogram-configuration/>'],
        new_contents=['<histogram-configuration/>'],
        action='M',
      ),
      histogram_validation.HistogramFileState(
        path=target_path,
        old_contents=['<histogram-configuration/>'],
        new_contents=['<histogram-configuration/>'],
        action='M',
      ),
    ]

    def read_file_fn(_path):
      return '<histogram-configuration></histogram-configuration>'

    result = histogram_validation.get_files_to_check(
      affected_files,
      read_file_fn,
      variants_paths=[],
      histograms_paths=[target_path],
    )
    self.assertEqual(len(result.files_to_check), 1)
    self.assertEqual(
      pathlib.Path(result.files_to_check[0].path), pathlib.Path(target_path)
    )

  @mock.patch(
    'chromium_src.tools.metrics.histograms.histogram_utils._path_contents'
  )
  def testGetFilesToCheck_VariantsModified(self, mock_path_contents):
    variants_path = str(
      (
        path_util.CHROMIUM_SRC_PATH
        / 'tools'
        / 'metrics'
        / 'histograms'
        / 'variants.xml'
      ).resolve()
    )
    histograms_path = str(
      (
        path_util.CHROMIUM_SRC_PATH
        / 'tools'
        / 'metrics'
        / 'histograms'
        / 'metadata'
        / 'uma'
        / 'histograms.xml'
      ).resolve()
    )

    affected_files = [
      histogram_validation.HistogramFileState(
        path=variants_path,
        old_contents=[
          '<histogram-configuration>',
          '<variants name="MockVariants">',
          '  <variant name="V1"/>',
          '</variants>',
          '</histogram-configuration>',
        ],
        new_contents=[
          '<histogram-configuration>',
          '<variants name="MockVariants">',
          '  <variant name="V1"/>',
          '  <variant name="V2"/>',
          '</variants>',
          '</histogram-configuration>',
        ],
        action='M',
      )
    ]

    histograms_content = (
      '<histogram-configuration>'
      '<histograms>'
      '  <histogram name="Test.{MockVariants}" enum="Boolean"/>'
      '</histograms>'
      '</histogram-configuration>'
    )

    def read_file_fn(path):
      if pathlib.Path(path) == pathlib.Path(histograms_path):
        return histograms_content
      return '<histogram-configuration></histogram-configuration>'

    mock_path_contents.side_effect = read_file_fn

    result = histogram_validation.get_files_to_check(
      affected_files,
      read_file_fn,
      variants_paths=[variants_path],
      histograms_paths=[histograms_path],
    )

    self.assertEqual(len(result.files_to_check), 1)
    self.assertEqual(
      pathlib.Path(result.files_to_check[0].path),
      pathlib.Path(histograms_path),
    )
    self.assertEqual(
      result.files_to_check[0].new_contents, histograms_content.splitlines()
    )
    self.assertIsNotNone(result.old_variants_doc)
    self.assertIsNotNone(result.new_variants_doc)

  def testGetOldAndNewVariants_Modified(self):
    target_path = str(pathlib.Path('/', 'src', 'variants1.xml'))
    affected = [
      histogram_validation.HistogramFileState(
        path=target_path,
        old_contents=['old1'],
        new_contents=['new1'],
        action='M',
      )
    ]

    def read_file_fn(_path):
      self.fail('Should not be called')

    res = histogram_validation.get_old_and_new_variants(
      affected, read_file_fn, [target_path]
    )
    self.assertEqual(len(res), 1)
    self.assertEqual(
      pathlib.Path(res[0].variants_file_path),
      pathlib.Path(target_path).resolve(),
    )
    self.assertEqual(res[0].old_variants, 'old1')
    self.assertEqual(res[0].new_variants, 'new1')
    self.assertTrue(res[0].is_modified)

  def testGetOldAndNewVariants_NotModified(self):
    target_path = str(pathlib.Path('/', 'src', 'variants1.xml'))
    affected = []

    def read_file_fn(path):
      if pathlib.Path(path) == pathlib.Path(target_path).resolve():
        return 'content1'
      return ''

    res = histogram_validation.get_old_and_new_variants(
      affected, read_file_fn, [target_path]
    )
    self.assertEqual(len(res), 1)
    self.assertEqual(
      pathlib.Path(res[0].variants_file_path),
      pathlib.Path(target_path).resolve(),
    )
    self.assertEqual(res[0].old_variants, 'content1')
    self.assertEqual(res[0].new_variants, 'content1')
    self.assertFalse(res[0].is_modified)

  def testGetHistogramNames_NoVariants(self):
    contents = [
      [
        '<histogram-configuration><histograms><histogram name="Foo"'
        ' units="ms"/></histograms></histogram-configuration>'
      ]
    ]
    names = histogram_validation.get_histogram_names(contents, variants=None)
    self.assertEqual(names, {'Foo'})

  def testGetHistogramNames_WithVariants(self):
    contents = [
      [
        '<histogram-configuration><histograms><histogram'
        ' name="Test.{MockVariants}"'
        ' units="ms"/></histograms></histogram-configuration>'
      ]
    ]
    variants_xml = (
      '<histogram-configuration><variants name="MockVariants">'
      '<variant name="V1"/><variant'
      ' name="V2"/></variants></histogram-configuration>'
    )
    variants_doc = ET.fromstring(variants_xml)

    names = histogram_validation.get_histogram_names(contents, variants_doc)
    self.assertEqual(names, {'Test.V1', 'Test.V2'})

  @mock.patch(
    'chromium_src.tools.metrics.histograms.histogram_utils._path_contents'
  )
  def testGetFilesToCheck_VariantsAdded(self, mock_path_contents):
    variants_path = str(
      (
        path_util.CHROMIUM_SRC_PATH
        / 'tools'
        / 'metrics'
        / 'histograms'
        / 'variants.xml'
      ).resolve()
    )
    histograms_path = str(
      (
        path_util.CHROMIUM_SRC_PATH
        / 'tools'
        / 'metrics'
        / 'histograms'
        / 'metadata'
        / 'uma'
        / 'histograms.xml'
      ).resolve()
    )

    # Simulates adding variants.xml (old_contents is empty)
    affected_files = [
      histogram_validation.HistogramFileState(
        path=variants_path,
        old_contents=[],
        new_contents=[
          '<histogram-configuration>',
          '<variants name="MockVariants">',
          '  <variant name="V1"/>',
          '</variants>',
          '</histogram-configuration>',
        ],
        action='A',
      )
    ]

    histograms_content = (
      '<histogram-configuration>'
      '<histograms>'
      '  <histogram name="Test.{MockVariants}" enum="Boolean"/>'
      '</histograms>'
      '</histogram-configuration>'
    )

    def read_file_fn(path):
      if pathlib.Path(path) == pathlib.Path(histograms_path):
        return histograms_content
      return ''

    mock_path_contents.side_effect = read_file_fn

    result = histogram_validation.get_files_to_check(
      affected_files,
      read_file_fn,
      variants_paths=[variants_path],
      histograms_paths=[histograms_path],
    )

    # It should detect that histograms_path is virtually affected
    self.assertEqual(len(result.files_to_check), 1)
    self.assertEqual(
      pathlib.Path(result.files_to_check[0].path),
      pathlib.Path(histograms_path),
    )
    self.assertIsNone(result.old_variants_doc)
    self.assertIsNotNone(result.new_variants_doc)

  @mock.patch(
    'chromium_src.tools.metrics.histograms.histogram_utils._path_contents'
  )
  def testGetFilesToCheck_VariantsDeleted(self, mock_path_contents):
    variants_path = str(
      (
        path_util.CHROMIUM_SRC_PATH
        / 'tools'
        / 'metrics'
        / 'histograms'
        / 'variants.xml'
      ).resolve()
    )
    histograms_path = str(
      (
        path_util.CHROMIUM_SRC_PATH
        / 'tools'
        / 'metrics'
        / 'histograms'
        / 'metadata'
        / 'uma'
        / 'histograms.xml'
      ).resolve()
    )

    # Simulates deleting variants.xml (new_contents is empty)
    affected_files = [
      histogram_validation.HistogramFileState(
        path=variants_path,
        old_contents=[
          '<histogram-configuration>',
          '<variants name="MockVariants">',
          '  <variant name="V1"/>',
          '</variants>',
          '</histogram-configuration>',
        ],
        new_contents=[],
        action='D',
      )
    ]

    histograms_content = (
      '<histogram-configuration>'
      '<histograms>'
      '  <histogram name="Test.{MockVariants}" enum="Boolean"/>'
      '</histograms>'
      '</histogram-configuration>'
    )

    def read_file_fn(path):
      if pathlib.Path(path) == pathlib.Path(histograms_path):
        return histograms_content
      return ''

    mock_path_contents.side_effect = read_file_fn

    result = histogram_validation.get_files_to_check(
      affected_files,
      read_file_fn,
      variants_paths=[variants_path],
      histograms_paths=[histograms_path],
    )

    # It should detect that histograms_path is virtually affected
    self.assertEqual(len(result.files_to_check), 1)
    self.assertEqual(
      pathlib.Path(result.files_to_check[0].path),
      pathlib.Path(histograms_path),
    )
    self.assertIsNotNone(result.old_variants_doc)
    self.assertIsNone(result.new_variants_doc)


if __name__ == '__main__':
  unittest.main()
