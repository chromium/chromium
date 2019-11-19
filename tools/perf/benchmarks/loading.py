# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark
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
    return tbm_options


@benchmark.Info(emails=['kouhei@chromium.org', 'ksakamoto@chromium.org'],
                component='Blink>Loader',
                documentation_url='https://bit.ly/loading-benchmarks')
class LoadingDesktop(_LoadingBase):
  """ A benchmark measuring loading performance of desktop sites. """
  SUPPORTED_PLATFORMS = [story.expectations.ALL_DESKTOP]

  def CreateStorySet(self, options):
    return page_sets.LoadingDesktopStorySet(
        cache_temperatures=[cache_temperature.COLD, cache_temperature.WARM])

  @classmethod
  def Name(cls):
    return 'loading.desktop'


@benchmark.Info(emails=['kouhei@chromium.org', 'ksakamoto@chromium.org'],
                component='Blink>Loader',
                documentation_url='https://bit.ly/loading-benchmarks')
class LoadingMobile(_LoadingBase):
  """ A benchmark measuring loading performance of mobile sites. """
  SUPPORTED_PLATFORMS = [story.expectations.ALL_MOBILE]

  def CreateStorySet(self, options):
    return page_sets.LoadingMobileStorySet(
        cache_temperatures=[cache_temperature.ANY],
        cache_temperatures_for_pwa=[cache_temperature.COLD,
                                    cache_temperature.WARM,
                                    cache_temperature.HOT],
        traffic_settings=[traffic_setting.NONE, traffic_setting.REGULAR_3G])

  @classmethod
  def Name(cls):
    return 'loading.mobile'
