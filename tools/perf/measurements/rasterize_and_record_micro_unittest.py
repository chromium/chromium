# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import decorators
from telemetry.testing import legacy_page_test_case

from measurements import rasterize_and_record_micro


class RasterizeAndRecordMicroUnitTest(legacy_page_test_case.LegacyPageTestCase):
  """Smoke test for rasterize_and_record_micro measurement

     Runs rasterize_and_record_micro measurement on a simple page and verifies
     that all metrics were added to the results. The test is purely functional,
     i.e. it only checks if the metrics are present and non-zero.
  """

  @decorators.Disabled('win', 'chromeos', 'linux')
  def testRasterizeAndRecordMicro(self):
    pate_test = rasterize_and_record_micro.RasterizeAndRecordMicro(
        rasterize_repeat=1, record_repeat=1, start_wait_time=0.0,
        report_detailed_results=True)
    measurements = self.RunPageTest(pate_test, 'file://blank.html')

    # For these measurements, a single positive scalar value is expected.
    expected_positve_scalar = [
        'rasterize_time',
        'record_time',
        'pixels_rasterized',
        'pixels_recorded',
        'pixels_rasterized_with_non_solid_color',
        'pixels_rasterized_as_opaque',
        'total_layers',
        'total_picture_layers',
        'total_picture_layers_with_no_content',
        'painter_memory_usage',
        'paint_op_memory_usage',
        'paint_op_count',
    ]
    for name in expected_positve_scalar:
      samples = measurements[name]['samples']
      self.assertEqual(len(samples), 1)
      self.assertGreater(samples[0], 0)

    samples = measurements['total_picture_layers_off_screen']['samples']
    self.assertEqual(len(samples), 1)
    self.assertEqual(samples[0], 0)
