# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark
from core import platforms

from telemetry import benchmark
from telemetry import story
from telemetry.timeline import chrome_trace_category_filter
from telemetry.web_perf import timeline_based_measurement
import page_sets


@benchmark.Info(
    emails=[
        'yuhengh@chromium.org', 'tluk@chromium.org'
    ],
    component='UI>Browser',
    documentation_url=
    'https://chromium.googlesource.com/chromium/src/+/main/docs/speed/benchmark/harnesses/desktop_ui.md'
)
class DesktopUI(perf_benchmark.PerfBenchmark):
  """Desktop UI Benchmark."""
  PLATFORM = 'desktop'
  SUPPORTED_PLATFORM_TAGS = [platforms.DESKTOP]
  SUPPORTED_PLATFORMS = [story.expectations.ALL_DESKTOP]

  def CreateStorySet(self, options):
    exhaustive = hasattr(options, 'story_set_should_be_exhaustive_for_test')
    return page_sets.DesktopUIStorySet(exhaustive=exhaustive)

  def CreateCoreTimelineBasedMeasurementOptions(self):
    category_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter(
        filter_string='uma')
    options = timeline_based_measurement.Options(category_filter)
    # Add more buffer since we are opening a lot of tabs.
    options.config.chrome_trace_config.SetTraceBufferSizeInKb(600 * 1024)
    options.SetTimelineBasedMetrics(['umaMetric'])
    return options

  def SetExtraBrowserOptions(self, options):
    # Make sure finch experiment is turned off for benchmarking.
    options.AppendExtraBrowserArgs('--enable-benchmarking')
    # UIDevtools is used for driving native UI.
    options.AppendExtraBrowserArgs('--enable-ui-devtools=0')
    options.AppendExtraBrowserArgs(
        '--enable-features=ui-debug-tools-enable-synthetic-events')

  @classmethod
  def Name(cls):
    return 'desktop_ui'
