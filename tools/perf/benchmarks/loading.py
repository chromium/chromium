# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark

from benchmarks import loading_metrics_category
from telemetry.web_perf import timeline_based_measurement


class _LoadingBase(perf_benchmark.PerfBenchmark):
  """ A base class for loading benchmarks. """

  options = {'pageset_repeat': 2}

  def CreateCoreTimelineBasedMeasurementOptions(self):
    tbm_options = timeline_based_measurement.Options()
    loading_metrics_category.AugmentOptionsForLoadingMetrics(tbm_options)
    # Enable "Memory.GPU.PeakMemoryUsage2.PageLoad" so we can measure the GPU
    # memory used throughout the page loading tests. Include "umaMetric" as a
    # timeline so that we can parse this UMA Histogram.
    tbm_options.config.chrome_trace_config.EnableUMAHistograms(
        'Memory.GPU.PeakMemoryUsage2.PageLoad',
        'PageLoad.PaintTiming.NavigationToLargestContentfulPaint',
        'PageLoad.PaintTiming.NavigationToFirstContentfulPaint',
        'PageLoad.LayoutInstability.CumulativeShiftScore')

    # Add "umaMetric" to the timeline based metrics. This does not override
    # those added in loading_metrics_category.AugmentOptionsForLoadingMetrics.
    tbm_options.AddTimelineBasedMetric('umaMetric')
    return tbm_options
