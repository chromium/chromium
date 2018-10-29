# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from core import perf_benchmark

from benchmarks import silk_flags
import page_sets
from telemetry import benchmark
from telemetry import story as story_module
from telemetry.timeline import chrome_trace_category_filter
from telemetry.web_perf import timeline_based_measurement


class _Smoothness(perf_benchmark.PerfBenchmark):
  """Base class for smoothness-based benchmarks."""

  @classmethod
  def Name(cls):
    return 'smoothness'

  def CreateCoreTimelineBasedMeasurementOptions(self):
    category_filter = chrome_trace_category_filter.CreateLowOverheadFilter()
    options = timeline_based_measurement.Options(category_filter)
    options.SetTimelineBasedMetrics(['renderingMetric'])
    return options


@benchmark.Info(emails=['bokan@chromium.org'], component='Blink>Scroll')
class SmoothnessToughPinchZoomCases(_Smoothness):
  """Measures rendering statistics for pinch-zooming in the tough pinch zoom
  cases.
  """
  page_set = page_sets.AndroidToughPinchZoomCasesPageSet
  SUPPORTED_PLATFORMS = [story_module.expectations.ALL_MOBILE]

  @classmethod
  def Name(cls):
    return 'smoothness.tough_pinch_zoom_cases'


@benchmark.Info(emails=['ericrk@chromium.org'])
class SmoothnessGpuRasterizationToughPinchZoomCases(_Smoothness):
  """Measures rendering statistics for pinch-zooming in the tough pinch zoom
  cases with GPU rasterization.
  """
  tag = 'gpu_rasterization'
  page_set = page_sets.AndroidToughPinchZoomCasesPageSet
  SUPPORTED_PLATFORMS = [story_module.expectations.ALL_MOBILE]

  def SetExtraBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)

  @classmethod
  def Name(cls):
    return 'smoothness.gpu_rasterization.tough_pinch_zoom_cases'
