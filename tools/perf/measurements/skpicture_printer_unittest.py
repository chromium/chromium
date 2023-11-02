# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import decorators
from telemetry.testing import legacy_page_test_case

from measurements import skpicture_printer


class SkpicturePrinterUnitTest(legacy_page_test_case.LegacyPageTestCase):
  # Picture printing is not supported on all platforms.
  @decorators.Disabled('android', 'chromeos')
  def testSkpicturePrinter(self):
    page_test = skpicture_printer.SkpicturePrinter(self.options.output_dir)
    measurements = self.RunPageTest(page_test, 'file://blank.html')
    saved_picture_count = measurements['saved_picture_count']['samples']
    self.assertEqual(len(saved_picture_count), 1)
    self.assertGreater(saved_picture_count[0], 0)
