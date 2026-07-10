# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import setup_modules  # pylint: disable=unused-import

import chromium_src.tools.metrics.histograms.histogram_validation as histogram_validation


class HistogramValidationTest(unittest.TestCase):

  def testCheckBooleansAreEnums_Failure(self):
    affected_files = [
        histogram_validation.AffectedFileForLineCheck(
            path='tools/metrics/histograms/metadata/uma/histograms.xml',
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
                'tools/metrics/histograms/metadata/uma/histograms.xml',
                10,
                '<histogram name="Foo" units="Boolean">',
            ),
            (
                'tools/metrics/histograms/metadata/uma/histograms.xml',
                12,
                '<histogram name="Bar" units="boolean">',
            ),
        ],
    )

  def testCheckBooleansAreEnums_Pass(self):
    affected_files = [
        histogram_validation.AffectedFileForLineCheck(
            path='tools/metrics/histograms/metadata/uma/histograms.xml',
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
        removed, segmentation)
    self.assertEqual(removed_seg, {'Segmentation.Foo'})

  def testCheckRemovedSegmentationHistograms_Pass(self):
    removed = {'Foo', 'Bar'}
    segmentation = {'Segmentation.Foo', 'Segmentation.Bar'}
    removed_seg = histogram_validation.check_removed_segmentation_histograms(
        removed, segmentation)
    self.assertEqual(removed_seg, set())

  def testCheckIfIntroducedTooManyHistograms_Failure(self):
    added = {'H1', 'H2', 'H3'}
    self.assertTrue(
        histogram_validation.check_if_introduced_too_many_histograms(
            added, threshold=2))

  def testCheckIfIntroducedTooManyHistograms_Pass(self):
    added = {'H1', 'H2'}
    self.assertFalse(
        histogram_validation.check_if_introduced_too_many_histograms(
            added, threshold=2))


if __name__ == '__main__':
  unittest.main()
