# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark
from core import platforms
import page_sets

from benchmarks import loading_metrics_category
from telemetry import benchmark
from telemetry import story
from telemetry.page import cache_temperature
from telemetry.page import traffic_setting
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


@benchmark.Info(emails=['blink-isolation-dev@chromium.org',
                        'kouhei@chromium.org'],
                component='Blink>Internals>Modularization',
                documentation_url='https://bit.ly/loading-benchmarks')
class LoadingMBI(_LoadingBase):
  """ A benchmark measuring loading performance of the sites the MBI team cares
  about. """
  # TODO(rmhasan): Remove the SUPPORTED_PLATFORMS lists.
  # SUPPORTED_PLATFORMS is deprecated, please put system specifier tags
  # from expectations.config in SUPPORTED_PLATFORM_TAGS.
  SUPPORTED_PLATFORM_TAGS = [platforms.MOBILE]
  SUPPORTED_PLATFORMS = [story.expectations.ALL_MOBILE]

  def CreateStorySet(self, options):
    return page_sets.LoadingMobileStorySet(
        cache_temperatures=[cache_temperature.ANY],
        cache_temperatures_for_pwa=[],
        traffic_settings=[traffic_setting.NONE, traffic_setting.REGULAR_3G],
        include_tags=['many_agents'])

  @classmethod
  def Name(cls):
    return 'UNSCHEDULED_loading.mbi'
