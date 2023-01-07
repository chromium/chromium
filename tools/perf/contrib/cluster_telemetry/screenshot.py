# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import time

import py_utils

from telemetry.page import legacy_page_test
from telemetry.util import image_util

class Screenshot(legacy_page_test.LegacyPageTest):
  """Takes a PNG screenshot of the page. If dc_detect flag is enabled, multiple
     screenshots will be taken, and only pixels that had the same RGB values
     across all screenshots will be saved to the final output. Inconsistent
     pixels will be made cyan."""

  def __init__(self, png_outdir, wait_time=0, dc_detect=False, dc_wait_time=1,
               dc_extra_screenshots=1, dc_threshold=0.5):
    super(Screenshot, self).__init__()
    self._png_outdir = png_outdir
    self._wait_time = wait_time
    self._dc_detect = dc_detect
    self._dc_wait_time = dc_wait_time
    self._dc_extra_screenshots = dc_extra_screenshots
    self._dc_threshold = dc_threshold

  def ValidateAndMeasurePage(self, page, tab, results):
    try:
      tab.WaitForDocumentReadyStateToBeComplete()
    except py_utils.TimeoutException:
      logging.warning("WaitForDocumentReadyStateToBeComplete() timeout, "
                      "page: %s", page.name)
      return

    time.sleep(self._wait_time)

    if not os.path.exists(self._png_outdir):
      logging.info("Creating directory %s", self._png_outdir)
      try:
        os.makedirs(self._png_outdir)
      except OSError:
        logging.warning("Directory %s could not be created", self._png_outdir)
        raise

    outpath = os.path.abspath(
        os.path.join(self._png_outdir, page.file_safe_name)) + '.png'
    # Replace win32 path separator char '\' with '\\'.
    outpath = outpath.replace('\\', '\\\\')

    screenshot = tab.Screenshot()
    image_width = image_util.Width(screenshot)
    image_height = image_util.Height(screenshot)
    num_total_pixels = image_width * image_height
    content_pixels = image_util.Pixels(screenshot)

    # Dynamic content flag.
    if self._dc_detect:
      for i in range(self._dc_extra_screenshots):
        logging.info("Sleeping for %f seconds.", self._dc_wait_time)
        time.sleep(self._dc_wait_time)

        # After the specified wait time, take another screenshot of the page.
        logging.info("Taking extra screenshot %d of %d.", i+1,
                     self._dc_extra_screenshots)
        next_screenshot = tab.Screenshot()

        # Compare this screenshot to the original to mark inconsistent pixels,
        # and check the percentage of dynamic content against the threshold.
        if not IsScreenshotWithinDynamicContentThreshold(
          screenshot, next_screenshot, content_pixels, num_total_pixels,
          self._dc_threshold):
          raise legacy_page_test.MeasurementFailure("Percentage of pixels "
            "with dynamic content is greater than threshold.")

    # Convert the pixel bytearray back into an image.
    image = image_util.FromRGBPixels(image_width, image_height,
                                     content_pixels)

    # TODO(lchoi): Add logging to image_util.py and/or augment error handling of
    # image_util.WritePngFile
    logging.info("Writing PNG file to %s. This may take awhile.", outpath)
    start = time.time()
    image_util.WritePngFile(image, outpath)
    logging.info("PNG file written successfully. (Took %f seconds)",
                 time.time()-start)

def IsScreenshotWithinDynamicContentThreshold(base_screenshot, next_screenshot,
  content_pixels, num_total_pixels, dynamic_content_threshold):
  """Compares two screenshot images and updates the bytearray of RGB values
     that will be used to create the final output image if it finds an
     inconsistency. Returns False if the percentage of pixels with dynamic
     content is greater than the given threshold, and True otherwise."""
  base_pixels = image_util.Pixels(base_screenshot)
  next_pixels = image_util.Pixels(next_screenshot)

  num_dynamic_pixels = 0.0
  i = 0
  while i < len(base_pixels):
    # If the RGB values of the two screenshots do not match, make the
    # corresponding pixel in the accumulated bytearray cyan and increment the
    # count of dynamic pixels.
    if (base_pixels[i] != next_pixels[i] or
        base_pixels[i+1] != next_pixels[i+1] or
        base_pixels[i+2] != next_pixels[i+2]):
      content_pixels[i] = 0
      content_pixels[i+1] = 255
      content_pixels[i+2] = 255
      num_dynamic_pixels += 1
    i += 3

  if (num_dynamic_pixels / num_total_pixels) >= dynamic_content_threshold:
    logging.error("Percentage of pixels with dynamic content: %f",
                  num_dynamic_pixels / num_total_pixels)
    return False
  return True
