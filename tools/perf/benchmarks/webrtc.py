# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import page_sets
from telemetry import benchmark
from telemetry.timeline import chrome_trace_category_filter
from telemetry.web_perf import timeline_based_measurement
from benchmarks import press


@benchmark.Info(emails=['video-cmi-apis@google.com', 'herre@google.com'],
                component='Blink>WebRTC',
                documentation_url='http://bit.ly/webrtc-benchmark')
class WebrtcPerfBenchmark(press._PressBenchmark):  # pylint: disable=protected-access
  """Base class for WebRTC metrics for real-time communications tests."""
  page_set = page_sets.WebrtcPageSet

  @classmethod
  def Name(cls):
    return 'webrtc'

  def CreateCoreTimelineBasedMeasurementOptions(self):
    categories = [
        # Disable all categories by default.
        '-*',
        'toplevel',
        # WebRTC
        'media',
    ]

    category_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter(
        filter_string=','.join(categories))
    options = timeline_based_measurement.Options(category_filter)
    options.config.chrome_trace_config.SetTraceBufferSizeInKb(300 * 1024)
    options.SetTimelineBasedMetrics([
        'cpuTimeMetric',
        'webrtcRenderingMetric',
    ])
    return options

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--use-fake-device-for-media-stream')
    options.AppendExtraBrowserArgs('--use-fake-ui-for-media-stream')
