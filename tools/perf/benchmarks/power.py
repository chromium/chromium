# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark
from core import platforms

import page_sets
from telemetry import benchmark
from telemetry import story
from telemetry.timeline import chrome_trace_category_filter
from telemetry.web_perf import timeline_based_measurement


@benchmark.Info(emails=['brucedawson@chromium.org'],
                documentation_url='https://bit.ly/power-benchmarks')
class PowerDesktop(perf_benchmark.PerfBenchmark):
  # TODO(johnchen): Remove either the SUPPORTED_PLATFORMS or
  # SUPPORTED_PLATFORMS_TAGS lists. Only one is necessary.
  SUPPORTED_PLATFORMS = [story.expectations.ALL_DESKTOP]
  SUPPORTED_PLATFORM_TAGS = [platforms.DESKTOP]

  def CreateStorySet(self, options):
    return page_sets.DesktopPowerStorySet()

  def CreateCoreTimelineBasedMeasurementOptions(self):
    category_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter(
        filter_string='toplevel')
    options = timeline_based_measurement.Options(category_filter)
    options.config.enable_chrome_trace = True
    options.config.enable_cpu_trace = True
    options.config.chrome_trace_config.SetTraceBufferSizeInKb(300 * 1024)
    options.SetTimelineBasedMetrics(['tbmv2:cpuTimeMetric',
                                     'tbmv3:console_error_metric'])
    return options

  @classmethod
  def Name(cls):
    return 'power.desktop'
