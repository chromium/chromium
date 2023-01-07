# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import decorators
from telemetry.testing import legacy_page_test_case

from measurements import multipage_skpicture_printer


class MultipageSkpicturePrinterUnitTest(
    legacy_page_test_case.LegacyPageTestCase):
  # Picture printing is not supported on all platforms.
  @decorators.Disabled('android', 'chromeos')
  def testSkpicturePrinter(self):
    page_test = multipage_skpicture_printer.MultipageSkpicturePrinter(
        self.options.output_dir)
    self.RunPageTest(page_test, 'file://blank.html')
