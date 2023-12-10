# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import platforms, perf_benchmark

from telemetry import benchmark, story
import page_sets
from benchmarks import v8_helper


class _V8LoadingBenchmark(v8_helper.V8PerfMixin, perf_benchmark.PerfBenchmark):
  """Base class for V8 loading benchmarks that measure RuntimeStats,
  eqt, gc and memory metrics.
  See loading_stories._LoadingStory for workload description.
  """

  def CreateStorySet(self, options):
    return page_sets.SystemHealthStorySet(platform=self.PLATFORM, case='load')


@benchmark.Info(emails=[
    'cbruni@chromium.org', 'vahl@chromium.org', 'almuthanna@chromium.org'
],
                component='Blink>JavaScript',
                documentation_url='https://bit.ly/system-health-v8-benchmarks')
class V8DesktopLoadingBenchmark(_V8LoadingBenchmark):
  """See _V8BrowsingBenchmark."""
  PLATFORM = 'desktop'
  # TODO(johnchen): Remove either the SUPPORTED_PLATFORMS or
  # SUPPORTED_PLATFORMS_TAGS lists. Only one is necessary.
  SUPPORTED_PLATFORMS = [story.expectations.ALL_DESKTOP]
  SUPPORTED_PLATFORM_TAGS = [platforms.DESKTOP]

  def CreateStorySet(self, options):
    return page_sets.SystemHealthStorySet(platform=self.PLATFORM, case='load')

  @classmethod
  def Name(cls):
    return 'UNSCHEDULED_v8.loading_desktop'


@benchmark.Info(emails=[
    'cbruni@chromium.org', 'leszeks@chromium.org'],
                component='Blink>JavaScript',
                documentation_url='https://bit.ly/system-health-v8-benchmarks')
class V8MobileLoadingBenchmark(_V8LoadingBenchmark):
  """See _V8BrowsingBenchmark."""
  PLATFORM = 'mobile'
  # TODO(johnchen): Remove either the SUPPORTED_PLATFORMS or
  # SUPPORTED_PLATFORMS_TAGS lists. Only one is necessary.
  SUPPORTED_PLATFORMS = [story.expectations.ALL_MOBILE]
  SUPPORTED_PLATFORM_TAGS = [platforms.MOBILE]

  @classmethod
  def Name(cls):
    return 'UNSCHEDULED_v8.loading_mobile'
