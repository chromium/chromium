# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from core import perf_benchmark


import page_sets

from telemetry import story
from telemetry import benchmark
from telemetry.timeline import chrome_trace_category_filter
from telemetry.web_perf import timeline_based_measurement


@benchmark.Info(emails=['cbruni@chromium.org', 'mythria@chromium.org'],
                component='Blink>JavaScript')
class V8Top25RuntimeStats(perf_benchmark.PerfBenchmark):
  """Runtime Stats benchmark for a 25 top V8 web pages.

  Designed to represent a mix between top websites and a set of pages that
  have unique V8 characteristics.
  """

  SUPPORTED_PLATFORMS = [story.expectations.ALL_DESKTOP]

  @classmethod
  def Name(cls):
    return 'v8.runtime_stats.top_25'

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs(
      '--enable-blink-features=BlinkRuntimeCallStats')

  def CreateCoreTimelineBasedMeasurementOptions(self):
    # TODO(fmeawad): most of the cat_filter information is extracted from
    # page_cycler_v2 TimelineBasedMeasurementOptionsForLoadingMetric because
    # used by the loadingMetric because the runtimeStatsMetric uses the
    # interactive time calculated internally by the loadingMetric.
    # It is better to share the code so that we can keep them in sync.
    cat_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter()

    # "blink.console" is used for marking ranges in
    # cache_temperature.MarkTelemetryInternal.
    cat_filter.AddIncludedCategory('blink.console')

    # "navigation" and "blink.user_timing" are needed to capture core
    # navigation events.
    cat_filter.AddIncludedCategory('navigation')
    cat_filter.AddIncludedCategory('blink.user_timing')

    # "loading" is needed for first-meaningful-paint computation.
    cat_filter.AddIncludedCategory('loading')

    # "toplevel" category is used to capture TaskQueueManager events
    # necessary to compute time-to-interactive.
    cat_filter.AddIncludedCategory('toplevel')

    # V8 needed categories
    cat_filter.AddIncludedCategory('v8')
    cat_filter.AddDisabledByDefault('disabled-by-default-v8.runtime_stats')

    tbm_options = timeline_based_measurement.Options(
        overhead_level=cat_filter)
    tbm_options.SetTimelineBasedMetrics(['runtimeStatsMetric'])
    return tbm_options

  def CreateStorySet(self, options):
    return page_sets.V8Top25StorySet()
