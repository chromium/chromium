# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import platforms, perf_benchmark

from telemetry import benchmark
from telemetry import story
import page_sets
from benchmarks import v8_helper


class _V8BrowsingBenchmark(v8_helper.V8PerfMixin, perf_benchmark.PerfBenchmark):
  """Base class for V8 browsing benchmarks that measure RuntimeStats,
  eqt, gc and memory metrics.
  See browsing_stories._BrowsingStory for workload description.
  """

  def CreateStorySet(self, options):
    return page_sets.SystemHealthStorySet(platform=self.PLATFORM, case='browse')


@benchmark.Info(emails=[
    'cbruni@chromium.org', 'vahl@chromium.org', 'almuthanna@chromium.org'
],
                component='Blink>JavaScript',
                documentation_url='https://bit.ly/system-health-v8-benchmarks')
class V8DesktopBrowsingBenchmark(_V8BrowsingBenchmark):
  """See _V8BrowsingBenchmark."""
  PLATFORM = 'desktop'
  # TODO(johnchen): Remove either the SUPPORTED_PLATFORMS or
  # SUPPORTED_PLATFORMS_TAGS lists. Only one is necessary.
  SUPPORTED_PLATFORMS = [story.expectations.ALL_DESKTOP]
  SUPPORTED_PLATFORM_TAGS = [platforms.DESKTOP]

  @classmethod
  def Name(cls):
    return 'v8.browsing_desktop'


@benchmark.Info(emails=[
    'cbruni@chromium.org', 'leszeks@chromium.org'],
                component='Blink>JavaScript',
                documentation_url='https://bit.ly/system-health-v8-benchmarks')
class V8MobileBrowsingBenchmark(
    _V8BrowsingBenchmark):
  """See _V8BrowsingBenchmark."""
  PLATFORM = 'mobile'
  # TODO(johnchen): Remove either the SUPPORTED_PLATFORMS or
  # SUPPORTED_PLATFORMS_TAGS lists. Only one is necessary.
  SUPPORTED_PLATFORMS = [story.expectations.ALL_MOBILE]
  SUPPORTED_PLATFORM_TAGS = [platforms.MOBILE]

  @classmethod
  def Name(cls):
    return 'v8.browsing_mobile'


@benchmark.Info(emails=[
    'cbruni@chromium.org', 'almuthanna@chromium.org'],
                component='Blink>JavaScript',
                documentation_url='https://bit.ly/system-health-v8-benchmarks')
class V8FutureDesktopBrowsingBenchmark(_V8BrowsingBenchmark):
  """See _V8BrowsingBenchmark."""
  PLATFORM = 'desktop'
  # TODO(johnchen): Remove either the SUPPORTED_PLATFORMS or
  # SUPPORTED_PLATFORMS_TAGS lists. Only one is necessary.
  SUPPORTED_PLATFORMS = [story.expectations.ALL_DESKTOP]
  SUPPORTED_PLATFORM_TAGS = [platforms.DESKTOP]

  def SetExtraBrowserOptions(self, options):
    super(V8FutureDesktopBrowsingBenchmark,
          self).SetExtraBrowserOptions(options)
    options.AppendExtraBrowserArgs('--enable-features=V8VmFuture')

  @classmethod
  def Name(cls):
    return 'v8.browsing_desktop-future'


@benchmark.Info(emails=[
    'cbruni@chromium.org', 'leszeks@chromium.org'],
                component='Blink>JavaScript',
                documentation_url='https://bit.ly/system-health-v8-benchmarks')
class V8FutureMobileBrowsingBenchmark(_V8BrowsingBenchmark):
  """See _V8BrowsingBenchmark."""
  PLATFORM = 'mobile'
  # TODO(johnchen): Remove either the SUPPORTED_PLATFORMS or
  # SUPPORTED_PLATFORMS_TAGS lists. Only one is necessary.
  SUPPORTED_PLATFORMS = [story.expectations.ALL_MOBILE]
  SUPPORTED_PLATFORM_TAGS = [platforms.MOBILE]

  def SetExtraBrowserOptions(self, options):
    super(V8FutureMobileBrowsingBenchmark, self).SetExtraBrowserOptions(options)
    options.AppendExtraBrowserArgs(
      '--enable-features=V8VmFuture')

  @classmethod
  def Name(cls):
    return 'v8.browsing_mobile-future'
