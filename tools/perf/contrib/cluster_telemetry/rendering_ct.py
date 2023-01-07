# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from contrib.cluster_telemetry import ct_benchmarks_util
from contrib.cluster_telemetry import page_set
from telemetry.timeline import chrome_trace_category_filter
from telemetry.web_perf import timeline_based_measurement

from core import perf_benchmark

import benchmarks.rendering as rendering

def ScrollToEndOfPage(action_runner):
  action_runner.Wait(1)
  with action_runner.CreateGestureInteraction('ScrollAction'):
    action_runner.ScrollPage()


class RenderingCT(perf_benchmark.PerfBenchmark):
  """Measures rendering performance for Cluster Telemetry."""

  options = {'upload_results': True}

  @classmethod
  def Name(cls):
    return 'rendering.cluster_telemetry'

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    ct_benchmarks_util.AddBenchmarkCommandLineArgs(parser)

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    ct_benchmarks_util.ValidateCommandLineArgs(parser, args)

  def CreateStorySet(self, options):
    return page_set.CTPageSet(
        options.urls_list, options.user_agent, options.archive_data_file,
        run_page_interaction_callback=ScrollToEndOfPage)

  def CreateCoreTimelineBasedMeasurementOptions(self):
    category_filter = chrome_trace_category_filter.CreateLowOverheadFilter()
    options = timeline_based_measurement.Options(category_filter)
    options.config.chrome_trace_config.EnableUMAHistograms(
        *rendering.RENDERING_BENCHMARK_UMA)
    options.SetTimelineBasedMetrics(['renderingMetric', 'umaMetric'])
    return options
