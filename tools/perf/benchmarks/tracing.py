# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark

from telemetry import benchmark
from telemetry.timeline import chrome_trace_category_filter
from telemetry.timeline import chrome_trace_config
from telemetry.web_perf import timeline_based_measurement

import page_sets


@benchmark.Info(emails=['ssid@chromium.org'])
class TracingWithBackgroundMemoryInfra(perf_benchmark.PerfBenchmark):
  """Measures the overhead of background memory-infra dumps"""
  page_set = page_sets.Top10PageSet

  def CreateCoreTimelineBasedMeasurementOptions(self):
    # Enable only memory-infra category with periodic background mode dumps
    # every 200 milliseconds.
    trace_memory = chrome_trace_category_filter.ChromeTraceCategoryFilter(
        filter_string='-*,blink.console,disabled-by-default-memory-infra')
    options = timeline_based_measurement.Options(overhead_level=trace_memory)
    memory_dump_config = chrome_trace_config.MemoryDumpConfig()
    memory_dump_config.AddTrigger('background', 200)
    options.config.chrome_trace_config.SetMemoryDumpConfig(memory_dump_config)
    options.SetTimelineBasedMetrics(['tracingMetric'])
    return options

  @classmethod
  def Name(cls):
    return 'tracing.tracing_with_background_memory_infra'
