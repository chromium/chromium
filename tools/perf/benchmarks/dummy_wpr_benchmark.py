# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Dummy benchmarks using WPR to test page load time.

The number produced isn't meant to represent any actual performance
data of the browser.
"""

from benchmarks import loading_metrics_category
from core import perf_benchmark
from page_sets import dummy_wpr_story_set
from telemetry import benchmark
from telemetry.web_perf import timeline_based_measurement


@benchmark.Info(emails=['maxqli@google.com'], component='Test>Telemetry')
class DummyWprLoadBenchmark(perf_benchmark.PerfBenchmark):
  options = {'pageset_repeat': 2}
  page_set = dummy_wpr_story_set.DummyWprStorySet

  def CreateCoreTimelineBasedMeasurementOptions(self):
    tbm_options = timeline_based_measurement.Options()
    loading_metrics_category.AugmentOptionsForLoadingMetrics(tbm_options)
    tbm_options.config.chrome_trace_config.EnableUMAHistograms(
        'PageLoad.PaintTiming.NavigationToLargestContentfulPaint',
        'PageLoad.PaintTiming.NavigationToFirstContentfulPaint',
        'PageLoad.LayoutInstability.CumulativeShiftScore')
    return tbm_options

  def CreateStorySet(self, options):
    return dummy_wpr_story_set.DummyWprStorySet()

  @classmethod
  def Name(cls):
    return 'UNSCHEDULED_dummy_wpr_benchmark.loading_using_wpr'
