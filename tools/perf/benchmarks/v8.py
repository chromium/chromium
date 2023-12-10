# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from core import perf_benchmark
from core import platforms

import page_sets

from telemetry import story
from telemetry import benchmark
from telemetry.timeline import chrome_trace_category_filter
from telemetry.web_perf import timeline_based_measurement


@benchmark.Info(
    emails=['cbruni@chromium.org', 'leszeks@chromium.org', 'vahl@chromium.org'],
    component='Blink>JavaScript')
class V8Top25RuntimeStats(perf_benchmark.PerfBenchmark):
  """Runtime Stats benchmark for a 25 top V8 web pages.

  Designed to represent a mix between top websites and a set of pages that
  have unique V8 characteristics.
  """
  # TODO(johnchen): Remove either the SUPPORTED_PLATFORMS or
  # SUPPORTED_PLATFORMS_TAGS lists. Only one is necessary.
  SUPPORTED_PLATFORMS = [story.expectations.ALL_DESKTOP]
  SUPPORTED_PLATFORM_TAGS = [platforms.DESKTOP]

  @classmethod
  def Name(cls):
    return 'v8.runtime_stats.top_25'

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs(
      '--enable-blink-features=BlinkRuntimeCallStats')

  def CreateCoreTimelineBasedMeasurementOptions(self):
    cat_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter()

    # "blink.console" is used for marking ranges in
    # cache_temperature.MarkTelemetryInternal.
    cat_filter.AddIncludedCategory('blink.console')

    # "toplevel" category is used to capture TaskQueueManager events.
    cat_filter.AddIncludedCategory('toplevel')

    # V8 needed categories
    cat_filter.AddIncludedCategory('v8')
    cat_filter.AddDisabledByDefault('disabled-by-default-v8.runtime_stats')

    tbm_options = timeline_based_measurement.Options(
        overhead_level=cat_filter)
    tbm_options.SetTimelineBasedMetrics(['runtimeStatsTotalMetric'])
    return tbm_options

  def CreateStorySet(self, options):
    return page_sets.V8Top25StorySet()
