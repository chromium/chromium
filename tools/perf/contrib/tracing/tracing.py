# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark

from telemetry.web_perf import timeline_based_measurement

import page_sets


class TracingWithDebugOverhead(perf_benchmark.PerfBenchmark):

  page_set = page_sets.Top10PageSet

  def CreateCoreTimelineBasedMeasurementOptions(self):
    options = timeline_based_measurement.Options(
        timeline_based_measurement.DEBUG_OVERHEAD_LEVEL)
    options.SetTimelineBasedMetrics(['tracingMetric'])
    return options

  @classmethod
  def Name(cls):
    return 'tracing.tracing_with_debug_overhead'
