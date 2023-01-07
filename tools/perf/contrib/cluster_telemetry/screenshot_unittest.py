# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from telemetry import decorators
from telemetry.testing import legacy_page_test_case
from telemetry.util import image_util
from contrib.cluster_telemetry import screenshot


class ScreenshotUnitTest(legacy_page_test_case.LegacyPageTestCase):
  # This test should only run on linux, but it got disabled on Linux as
  # well because of crbug.com/1023255. Replace the following with
  # @decorators.Enabled('linux') once this is fixed on linux.
  @decorators.Disabled('all')
  def testScreenshot(self):
    # Screenshots for Cluster Telemetry purposes currently only supported on
    # Linux platform.
    screenshot_test = screenshot.Screenshot(self.options.output_dir)
    self.RunPageTest(screenshot_test, 'file://screenshot_test.html')

    filepath = os.path.join(self.options.output_dir, 'screenshot_test.png')
    self.assertTrue(os.path.exists(filepath))
    self.assertTrue(os.path.isfile(filepath))
    self.assertTrue(os.access(filepath, os.R_OK))

    image = image_util.FromPngFile(filepath)
    screenshot_pixels = image_util.Pixels(image)
    special_colored_pixel = bytearray([217, 115, 43])
    self.assertTrue(special_colored_pixel in screenshot_pixels)

  @decorators.Enabled('linux')
  def testIsScreenshotWithinDynamicContentThreshold(self):
    # TODO(lchoi): This unit test fails on Windows due to an apparent platform
    # dependent image decoding behavior that will need to be investigated in the
    # future if Cluster Telemetry ever becomes compatible with Windows.
    width = 2
    height = 1
    num_total_pixels = width * height

    content_pixels = bytearray([0, 0, 0, 128, 128, 128])
    base_screenshot = image_util.FromRGBPixels(width, height, content_pixels)

    next_pixels = bytearray([1, 1, 1, 128, 128, 128])
    next_screenshot = image_util.FromRGBPixels(width, height, next_pixels)
    expected_pixels = bytearray([0, 255, 255, 128, 128, 128])
    self.assertTrue(screenshot.IsScreenshotWithinDynamicContentThreshold(
                    base_screenshot, next_screenshot, content_pixels,
                    num_total_pixels, 0.51))
    self.assertTrue(expected_pixels == content_pixels)

    next_pixels = bytearray([0, 0, 0, 1, 1, 1])
    next_screenshot = image_util.FromRGBPixels(2, 1, next_pixels)
    expected_pixels = bytearray([0, 255, 255, 0, 255, 255])
    self.assertTrue(screenshot.IsScreenshotWithinDynamicContentThreshold(
                    base_screenshot, next_screenshot, content_pixels,
                    num_total_pixels, 0.51))
    self.assertTrue(expected_pixels == content_pixels)
    self.assertFalse(screenshot.IsScreenshotWithinDynamicContentThreshold(
                     base_screenshot, next_screenshot, content_pixels,
                     num_total_pixels, 0.49))
