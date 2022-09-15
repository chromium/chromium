# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for translation_helper.py."""
import unittest
import os
import sys

import translation_helper

here = os.path.realpath(__file__)
testdata_path = os.path.normpath(os.path.join(here, '..', '..', 'testdata'))


class TcHelperTest(unittest.TestCase):

  def test_get_translatable_grds(self):
    grds = translation_helper.get_translatable_grds(
        testdata_path, ['test.grd', 'not_translated.grd', 'internal.grd'],
        os.path.join(testdata_path,
                     'translation_expectations_without_unlisted_file.pyl'))
    self.assertEqual(1, len(grds))

    # There should be no references to not_translated.grd (mentioning the
    # filename here so that it doesn't appear unused).
    grd = grds[0]
    self.assertEqual(os.path.join(testdata_path, 'test.grd'), grd.path)
    self.assertEqual(testdata_path, grd.dir)
    self.assertEqual('test.grd', grd.name)
    self.assertEqual([os.path.join(testdata_path, 'part.grdp')], grd.grdp_paths)
    self.assertEqual([], grd.structure_paths)
    self.assertEqual([os.path.join(testdata_path, 'test_en-GB.xtb')],
                     grd.xtb_paths)
    self.assertEqual({'en-GB': os.path.join(testdata_path, 'test_en-GB.xtb')},
                     grd.lang_to_xtb_path)
    self.assertTrue(grd.appears_translatable)
    self.assertEquals(['en-GB'], grd.expected_languages)

  # The expectations list an untranslatable file (not_translated.grd), but the
  # grd list doesn't contain it.
  def test_missing_untranslatable(self):
    TRANSLATION_EXPECTATIONS = os.path.join(
        testdata_path, 'translation_expectations_without_unlisted_file.pyl')
    with self.assertRaises(Exception) as context:
      translation_helper.get_translatable_grds(
          testdata_path, ['test.grd', 'internal.grd'], TRANSLATION_EXPECTATIONS)
    self.assertEqual(
        '%s needs to be updated. Please fix these issues:\n'
        ' - not_translated.grd is listed in the translation expectations, '
        'but this grd file does not exist.' % TRANSLATION_EXPECTATIONS,
        str(context.exception))

  # The expectations list an internal file (internal.grd), but the grd list
  # doesn't contain it.
  def test_missing_internal(self):
    TRANSLATION_EXPECTATIONS = os.path.join(
        testdata_path, 'translation_expectations_without_unlisted_file.pyl')
    with self.assertRaises(Exception) as context:
      translation_helper.get_translatable_grds(
          testdata_path, ['test.grd', 'not_translated.grd'],
          TRANSLATION_EXPECTATIONS)
    self.assertEqual(
        '%s needs to be updated. Please fix these issues:\n'
        ' - internal.grd is listed in translation expectations as an internal '
        'file to be ignored, but this grd file does not exist.' %
        TRANSLATION_EXPECTATIONS, str(context.exception))

  # The expectations list a translatable file (test.grd), but the grd list
  # doesn't contain it.
  def test_missing_translatable(self):
    TRANSLATION_EXPECTATIONS = os.path.join(
        testdata_path, 'translation_expectations_without_unlisted_file.pyl')
    with self.assertRaises(Exception) as context:
      translation_helper.get_translatable_grds(
          testdata_path, ['not_translated.grd', 'internal.grd'],
          TRANSLATION_EXPECTATIONS)
    self.assertEqual(
        '%s needs to be updated. Please fix these issues:\n'
        ' - test.grd is listed in the translation expectations, but this grd '
        'file does not exist.' % TRANSLATION_EXPECTATIONS,
        str(context.exception))

  # The grd list contains a file (part.grdp) that's not listed in translation
  # expectations.
  def test_expectations_not_updated(self):
    TRANSLATION_EXPECTATIONS = os.path.join(
        testdata_path, 'translation_expectations_without_unlisted_file.pyl')
    with self.assertRaises(Exception) as context:
      translation_helper.get_translatable_grds(
          testdata_path,
          ['test.grd', 'part.grdp', 'not_translated.grd', 'internal.grd'],
          TRANSLATION_EXPECTATIONS)
    self.assertEqual(
        '%s needs to be updated. Please fix these issues:\n'
        ' - part.grdp appears to be translatable (because it contains <file> '
        'or <message> elements), but is not listed in the translation '
        'expectations.' % TRANSLATION_EXPECTATIONS, str(context.exception))


if __name__ == '__main__':
  unittest.main()
