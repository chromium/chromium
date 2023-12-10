# Copyright 2015 The Chromium Authors
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


def CreateCoreTimelineBasedMemoryMeasurementOptions():
  """Creates necessary TBM options for measuring memory usage.

  Separated out so that code can be re-used in other benchmarks.
  """
  # Enable only memory-infra, to get memory dumps, and blink.console, to get
  # the timeline markers used for mapping threads to tabs.
  trace_memory = chrome_trace_category_filter.ChromeTraceCategoryFilter(
      filter_string='-*,blink.console,disabled-by-default-memory-infra')
  tbm_options = timeline_based_measurement.Options(
      overhead_level=trace_memory)
  tbm_options.config.enable_android_graphics_memtrack = True
  tbm_options.SetTimelineBasedMetrics(['memoryMetric'])
  # Setting an empty memory dump config disables periodic dumps.
  tbm_options.config.chrome_trace_config.SetMemoryDumpConfig(
      chrome_trace_config.MemoryDumpConfig())
  return tbm_options


def SetExtraBrowserOptionsForMemoryMeasurement(options):
  """Sets extra browser args necessary for measuring memory usage.

  Separated out so that code can be re-used in other benchmarks.
  """
  # Just before we measure memory we flush the system caches
  # unfortunately this doesn't immediately take effect, instead
  # the next page run is effected. Due to this the first page run
  # has anomalous results. This option causes us to flush caches
  # each time before Chrome starts so we effect even the first page
  # - avoiding the bug.
  options.flush_os_page_caches_on_start = True


@benchmark.Info(emails=['lizeb@chromium.org'])
class MemoryBenchmarkDesktop(perf_benchmark.PerfBenchmark):
  """Measure memory usage on synthetic sites."""
  options = {'pageset_repeat': 5}
  # TODO(johnchen): Remove either the SUPPORTED_PLATFORMS or
  # SUPPORTED_PLATFORMS_TAGS lists. Only one is necessary.
  SUPPORTED_PLATFORMS = [story.expectations.ALL_DESKTOP]
  SUPPORTED_PLATFORM_TAGS = [platforms.DESKTOP]

  def CreateCoreTimelineBasedMeasurementOptions(self):
    return CreateCoreTimelineBasedMemoryMeasurementOptions()

  def SetExtraBrowserOptions(self, options):
    SetExtraBrowserOptionsForMemoryMeasurement(options)

  def CreateStorySet(self, options):
    story_set = page_sets.TrivialSitesStorySet(wait_in_seconds=0,
        measure_memory=True)
    for page in page_sets.WebWorkerStorySet(measure_memory=True):
      story_set.AddStory(page)
    return story_set

  @classmethod
  def Name(cls):
    return 'memory.desktop'
