# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import glob
import os

from telemetry.page import legacy_page_test


class SkpicturePrinter(legacy_page_test.LegacyPageTest):

  def __init__(self, skp_outdir):
    super(SkpicturePrinter, self).__init__()
    self._skp_outdir = skp_outdir

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs(['--enable-gpu-benchmarking',
                                    '--no-sandbox',
                                    '--enable-deferred-image-decoding'])

  def ValidateAndMeasurePage(self, page, tab, results):
    if tab.browser.platform.GetOSName() in ['android', 'chromeos']:
      raise legacy_page_test.MeasurementFailure(
          'SkPicture printing not supported on this platform')

    outpath = os.path.abspath(
        os.path.join(self._skp_outdir, page.file_safe_name))
    # Replace win32 path separator char '\' with '\\'.
    outpath = outpath.replace('\\', '\\\\')
    tab.EvaluateJavaScript(
        'chrome.gpuBenchmarking.printToSkPicture({{ outpath }});',
        outpath=outpath)
    pictures = glob.glob(os.path.join(outpath, '*.skp'))
    results.AddMeasurement('saved_picture_count', 'count', len(pictures))
