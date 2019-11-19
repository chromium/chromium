# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from core import perf_benchmark

import page_sets
from telemetry import benchmark
from telemetry import story as story_module
from telemetry.timeline import chrome_trace_category_filter
from telemetry.web_perf import timeline_based_measurement

@benchmark.Info(emails=['chiniforooshan@chromium.org', 'sadrul@chromium.org'])
class CrosUiSmoothnessBenchmark(perf_benchmark.PerfBenchmark):
  """Measures ChromeOS UI smoothness."""
  page_set = page_sets.CrosUiCasesPageSet
  SUPPORTED_PLATFORMS = [story_module.expectations.ALL_CHROMEOS]

  @classmethod
  def Name(cls):
    return 'cros_ui_smoothness'

  def CreateCoreTimelineBasedMeasurementOptions(self):
    category_filter = chrome_trace_category_filter.CreateLowOverheadFilter()
    options = timeline_based_measurement.Options(category_filter)
    options.config.chrome_trace_config.EnableUMAHistograms(
        'Event.Latency.ScrollBegin.Touch.TimeToScrollUpdateSwapBegin4',
        'Event.Latency.ScrollUpdate.Touch.TimeToScrollUpdateSwapBegin4')
    options.SetTimelineBasedMetrics(['renderingMetric', 'umaMetric'])
    return options
