# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

from telemetry.page import legacy_page_test

import py_utils

class RasterizeAndRecordMicro(legacy_page_test.LegacyPageTest):

  def __init__(self, start_wait_time=2, rasterize_repeat=100, record_repeat=100,
               timeout=120, report_detailed_results=False):
    super(RasterizeAndRecordMicro, self).__init__()
    self._chrome_branch_number = None
    self._start_wait_time = start_wait_time
    self._rasterize_repeat = rasterize_repeat
    self._record_repeat = record_repeat
    self._timeout = timeout
    self._report_detailed_results = report_detailed_results

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs([
        '--enable-gpu-benchmarking'
    ])

  def ValidateAndMeasurePage(self, page, tab, results):
    del page  # unused
    try:
      tab.WaitForDocumentReadyStateToBeComplete()
    except py_utils.TimeoutException:
      pass
    time.sleep(self._start_wait_time)

    # Enqueue benchmark
    tab.ExecuteJavaScript("""
        window.benchmark_results = {};
        window.benchmark_results.done = false;
        window.benchmark_results.id =
            chrome.gpuBenchmarking.runMicroBenchmark(
                "rasterize_and_record_benchmark",
                function(value) {
                  window.benchmark_results.done = true;
                  window.benchmark_results.results = value;
                }, {
                  "record_repeat_count": {{ record_repeat_count }},
                  "rasterize_repeat_count": {{ rasterize_repeat_count }}
                });
        """,
        record_repeat_count=self._record_repeat,
        rasterize_repeat_count=self._rasterize_repeat)

    # Evaluating this expression usually takes between 60 and 90 seconds.
    benchmark_id = tab.EvaluateJavaScript(
        'window.benchmark_results.id', timeout=self._timeout)
    if not benchmark_id:
      raise legacy_page_test.MeasurementFailure(
          'Failed to schedule rasterize_and_record_micro')

    tab.WaitForJavaScriptCondition(
        'window.benchmark_results.done', timeout=self._timeout)

    data = tab.EvaluateJavaScript('window.benchmark_results.results')

    pixels_recorded = data['pixels_recorded']
    pixels_rasterized = data['pixels_rasterized']
    painter_memory_usage = data.get('painter_memory_usage', 0)
    paint_op_memory_usage = data.get('paint_op_memory_usage', 0)
    paint_op_count = data.get('paint_op_count', 0)

    results.AddMeasurement('pixels_recorded', 'count', pixels_recorded)
    results.AddMeasurement('pixels_rasterized', 'count', pixels_rasterized)
    results.AddMeasurement('painter_memory_usage', 'bytes',
                           painter_memory_usage)
    results.AddMeasurement('paint_op_memory_usage', 'bytes',
                           paint_op_memory_usage)
    results.AddMeasurement('paint_op_count', 'count', paint_op_count)

    for metric in ('rasterize_time', 'record_time',
                   'record_time_caching_disabled',
                   'record_time_subsequence_caching_disabled',
                   'raster_invalidation_and_convert_time',
                   'paint_artifact_compositor_update_time'):
      results.AddMeasurement(metric, 'ms', data.get(metric + '_ms', 0))

    if self._report_detailed_results:
      for metric in ('pixels_rasterized_with_non_solid_color',
                     'pixels_rasterized_as_opaque', 'total_layers',
                     'total_picture_layers',
                     'total_picture_layers_with_no_content',
                     'total_picture_layers_off_screen'):
        results.AddMeasurement(metric, 'count', data[metric])

      lcd_text_pixels = data['visible_pixels_by_lcd_text_disallowed_reason']
      for reason in lcd_text_pixels:
        if reason == 'none':
          name = 'visible_pixels_lcd_text'
        else:
          name = 'visible_pixels_non_lcd_text:' + reason
        results.AddMeasurement(name, 'count', lcd_text_pixels[reason])
