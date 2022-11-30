#!/usr/bin/env python
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import os
import sys

here = os.path.realpath(__file__)
testdata_path = os.path.normpath(os.path.join(here, '..', 'testdata'))

import upload_screenshots


class UploadTests(unittest.TestCase):

  def test_find_screenshots(self):
    screenshots = upload_screenshots.find_screenshots(
        testdata_path,
        os.path.join(testdata_path, 'translation_expectations.pyl'))
    self.assertEqual(2, len(screenshots))
    self.assertEqual(
        os.path.join(testdata_path, 'test_grd', 'IDS_TEST_STRING1.png'),
        screenshots[0])
    self.assertEqual(
        os.path.join(testdata_path, 'part_grdp', 'IDS_PART_STRING2.png'),
        screenshots[1])


if __name__ == '__main__':
  unittest.main()
