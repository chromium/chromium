# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark
from core import platforms

from telemetry import benchmark
from telemetry import story
from telemetry.timeline import chrome_trace_category_filter
from telemetry.web_perf import timeline_based_measurement
import page_sets

TAB_SEARCH_BENCHMARK_UMA = [
    'Tabs.TabSearch.CloseAction',
    'Tabs.TabSearch.NumTabsClosedPerInstance',
    'Tabs.TabSearch.NumTabsOnOpen',
    'Tabs.TabSearch.NumWindowsOnOpen',
    'Tabs.TabSearch.OpenAction',
    'Tabs.TabSearch.PageHandlerConstructionDelay',
    'Tabs.TabSearch.WebUI.InitialTabsRenderTime',
    'Tabs.TabSearch.WebUI.LoadCompletedTime',
    'Tabs.TabSearch.WebUI.LoadDocumentTime',
    'Tabs.TabSearch.WebUI.TabListDataReceived',
    'Tabs.TabSearch.WebUI.TabSwitchAction',
    'Tabs.TabSearch.WindowDisplayedDuration2',
    'Tabs.TabSearch.WindowTimeToShowCachedWebView',
    'Tabs.TabSearch.WindowTimeToShowUncachedWebView',
]


@benchmark.Info(
    emails=[
        'yuhengh@chromium.org', 'tluk@chromium.org', 'romanarora@chromium.org'
    ],
    component='UI>Browser>TabSearch',
    documentation_url=
    'https://chromium.googlesource.com/chromium/src/+/master/docs/speed/benchmark/harnesses/tab_search.md'
)
class TabSearch(perf_benchmark.PerfBenchmark):
  """Tab Search Benchmark."""
  PLATFORM = 'desktop'
  SUPPORTED_PLATFORM_TAGS = [platforms.DESKTOP]
  SUPPORTED_PLATFORMS = [story.expectations.ALL_DESKTOP]

  def CreateStorySet(self, options):
    return page_sets.TabSearchStorySet()

  def CreateCoreTimelineBasedMeasurementOptions(self):
    category_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter(
        filter_string='uma')
    category_filter.AddIncludedCategory('browser')
    category_filter.AddIncludedCategory('blink.user_timing')
    options = timeline_based_measurement.Options(category_filter)
    options.config.chrome_trace_config.EnableUMAHistograms(
        *TAB_SEARCH_BENCHMARK_UMA)
    # Add more buffer since we are opening a lot of tabs.
    options.config.chrome_trace_config.SetTraceBufferSizeInKb(600 * 1024)
    options.SetTimelineBasedMetrics(['webuiMetric', 'umaMetric'])
    return options

  @classmethod
  def Name(cls):
    return 'tab_search'
