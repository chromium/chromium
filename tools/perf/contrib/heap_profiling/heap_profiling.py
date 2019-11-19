# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from benchmarks import loading_metrics_category

from core import perf_benchmark

from telemetry import story
from telemetry.timeline import chrome_trace_category_filter
from telemetry.timeline import chrome_trace_config
from telemetry.web_perf import timeline_based_measurement

import page_sets
from page_sets.system_health import loading_stories


_PAGE_SETS_DATA = os.path.join(os.path.dirname(page_sets.__file__), 'data')


class _HeapProfilingStorySet(story.StorySet):
  """Small story set containing loading stories and invoking memory dumps."""
  def __init__(self, platform):
    super(_HeapProfilingStorySet, self).__init__(
        archive_data_file=
            os.path.join(_PAGE_SETS_DATA, 'system_health_%s.json' % platform),
        cloud_storage_bucket=story.PARTNER_BUCKET)
    self.AddStory(
        loading_stories.LoadGoogleStory2018(self, take_memory_measurement=True))
    self.AddStory(
        loading_stories.LoadTwitterStory(self, take_memory_measurement=True))
    self.AddStory(
        loading_stories.LoadCnnStory2018(self, take_memory_measurement=True))


class _HeapProfilingBenchmark(perf_benchmark.PerfBenchmark):
  """Bechmark to measure heap profiling overhead."""
  PROFILING_MODE = NotImplemented
  PLATFORM = NotImplemented

  def CreateCoreTimelineBasedMeasurementOptions(self):
    cat_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter(
        filter_string='-*,toplevel,rail,disabled-by-default-memory-infra')
    options = timeline_based_measurement.Options(cat_filter)
    options.SetTimelineBasedMetrics([
        'cpuTimeMetric',
        'loadingMetric',
        'memoryMetric',
        'tracingMetric',
    ])
    loading_metrics_category.AugmentOptionsForLoadingMetrics(options)
    # Disable periodic dumps by setting default config.
    options.config.chrome_trace_config.SetMemoryDumpConfig(
        chrome_trace_config.MemoryDumpConfig())
    return options

  def CreateStorySet(self, options):
    return _HeapProfilingStorySet(self.PLATFORM)

  def SetExtraBrowserOptions(self, options):
    super(_HeapProfilingBenchmark, self).SetExtraBrowserOptions(options)
    args = []
    if self.PROFILING_MODE == 'pseudo':
      args += [
          '--memlog=all', '--memlog-stack-mode=pseudo', '--memlog-sampling']
    elif self.PROFILING_MODE == 'native':
      args += [
          '--memlog=all', '--memlog-stack-mode=native-with-thread-names',
          '--memlog-sampling']
    options.AppendExtraBrowserArgs(args)


class PseudoHeapProfilingDesktopBenchmark(_HeapProfilingBenchmark):
  PROFILING_MODE = 'pseudo'
  PLATFORM = 'desktop'
  SUPPORTED_PLATFORMS = [story.expectations.ALL_DESKTOP]

  @classmethod
  def Name(cls):
    return 'heap_profiling.desktop.pseudo'


class NativeHeapProfilingDesktopBenchmark(_HeapProfilingBenchmark):
  PROFILING_MODE = 'native'
  PLATFORM = 'desktop'
  SUPPORTED_PLATFORMS = [story.expectations.ALL_DESKTOP]

  @classmethod
  def Name(cls):
    return 'heap_profiling.desktop.native'


class DisabledHeapProfilingDesktopBenchmark(_HeapProfilingBenchmark):
  PROFILING_MODE = 'disabled'
  PLATFORM = 'desktop'
  SUPPORTED_PLATFORMS = [story.expectations.ALL_DESKTOP]

  @classmethod
  def Name(cls):
    return 'heap_profiling.desktop.disabled'


class PseudoHeapProfilingMobileBenchmark(_HeapProfilingBenchmark):
  PROFILING_MODE = 'pseudo'
  PLATFORM = 'mobile'
  SUPPORTED_PLATFORMS = [story.expectations.ALL_MOBILE]

  @classmethod
  def Name(cls):
    return 'heap_profiling.mobile.pseudo'


class NativeHeapProfilingMobileBenchmark(_HeapProfilingBenchmark):
  PROFILING_MODE = 'native'
  PLATFORM = 'mobile'
  SUPPORTED_PLATFORMS = [story.expectations.ALL_MOBILE]

  @classmethod
  def Name(cls):
    return 'heap_profiling.mobile.native'


class DisabledHeapProfilingMobileBenchmark(_HeapProfilingBenchmark):
  PROFILING_MODE = 'disabled'
  PLATFORM = 'mobile'
  SUPPORTED_PLATFORMS = [story.expectations.ALL_MOBILE]

  @classmethod
  def Name(cls):
    return 'heap_profiling.mobile.disabled'
