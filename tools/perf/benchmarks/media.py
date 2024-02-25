# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark
from core import platforms

from telemetry import benchmark
from telemetry import story
from telemetry.timeline import chrome_trace_category_filter
from telemetry.timeline import chrome_trace_config
from telemetry.web_perf import timeline_based_measurement

import page_sets


# TODO(rnephew): Revisit the re-enabled benchmarks on Wed, Aug 8 2017.


class _MediaBenchmark(perf_benchmark.PerfBenchmark):
  """Base class for TBMv2-based media benchmarks (MediaDesktop and
  MediaMobile)."""

  def CreateCoreTimelineBasedMeasurementOptions(self):
    category_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter()

    # 'toplevel' category provides CPU time slices used by # cpuTimeMetric.
    category_filter.AddIncludedCategory('toplevel')

    # Collect media related events required by mediaMetric.
    category_filter.AddIncludedCategory('media')

    # Collect memory data.
    category_filter.AddDisabledByDefault('disabled-by-default-memory-infra')

    options = timeline_based_measurement.Options(category_filter)
    options.config.enable_android_graphics_memtrack = True

    # Setting an empty memory dump config disables periodic dumps.
    options.config.chrome_trace_config.SetMemoryDumpConfig(
        chrome_trace_config.MemoryDumpConfig())

    # Note that memoryMetric is added using GetExtraTracingMetrics() for
    # certain stories.
    options.SetTimelineBasedMetrics(['mediaMetric', 'cpuTimeMetric'])
    return options

  def SetExtraBrowserOptions(self, options):
    # bf-cache messes with the time_to_play numbers when we do several runs
    # in a row. More info crbug.com/1309294
    options.AppendExtraBrowserArgs('--disable-features=BackForwardCache')


@benchmark.Info(emails=['dalecurtis@chromium.org'],
                component='Internals>Media',
                documentation_url='https://chromium.googlesource.com/chromium/src/+/main/docs/speed/benchmark/harnesses/media.md')  # pylint: disable=line-too-long
class MediaDesktop(_MediaBenchmark):
  """Obtains media performance for key user scenarios on desktop."""
  # TODO(johnchen): Remove either the SUPPORTED_PLATFORMS or
  # SUPPORTED_PLATFORMS_TAGS lists. Only one is necessary.
  SUPPORTED_PLATFORM_TAGS = [platforms.DESKTOP]
  SUPPORTED_PLATFORMS = [story.expectations.ALL_DESKTOP]

  def CreateStorySet(self, options):
    return page_sets.MediaCasesDesktopStorySet()

  @classmethod
  def Name(cls):
    return 'media.desktop'


# If any story is failing on svelte, please only disable on svelte.
@benchmark.Info(emails=['dalecurtis@chromium.org'],
                component='Internals>Media',
                documentation_url='https://chromium.googlesource.com/chromium/src/+/main/docs/speed/benchmark/harnesses/media.md')  # pylint: disable=line-too-long
class MediaMobile(_MediaBenchmark):
  """Obtains media performance for key user scenarios on mobile devices."""

  # TODO(johnchen): Remove either the SUPPORTED_PLATFORMS or
  # SUPPORTED_PLATFORMS_TAGS lists. Only one is necessary.
  SUPPORTED_PLATFORM_TAGS = [
      platforms.ANDROID_NOT_WEBVIEW, platforms.FUCHSIA_ASTRO,
      platforms.FUCHSIA_SHERLOCK
  ]
  SUPPORTED_PLATFORMS = [
      story.expectations.ANDROID_NOT_WEBVIEW, story.expectations.FUCHSIA_ASTRO,
      story.expectations.FUCHSIA_SHERLOCK
  ]

  def CreateStorySet(self, options):
    return page_sets.MediaCasesMobileStorySet()

  @classmethod
  def Name(cls):
    return 'media.mobile'

  def SetExtraBrowserOptions(self, options):
    super(MediaMobile, self).SetExtraBrowserOptions(options)
    # By default, Chrome on Android does not allow autoplay
    # of media: it requires a user gesture event to start a video.
    # The following option works around that.
    options.AppendExtraBrowserArgs(
        ['--autoplay-policy=no-user-gesture-required'])
    # Force online state for the offline indicator so it doesn't show and affect
    # the benchmarks on bots, which are offline by default.
    options.AppendExtraBrowserArgs(
        '--force-online-connection-state-for-indicator')
