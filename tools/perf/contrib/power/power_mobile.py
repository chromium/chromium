# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark
from core import platforms
from contrib.power.top_sites_story import ContribPowerMobileTopSitesStorySet
from telemetry import benchmark
from telemetry import story
from telemetry.timeline.chrome_trace_category_filter import ChromeTraceCategoryFilter
from telemetry.web_perf import timeline_based_measurement


@benchmark.Info(emails=['chrometto-team@google.com'])
class ContribPowerMobile(perf_benchmark.PerfBenchmark):

  SUPPORTED_PLATFORMS = [story.expectations.ALL_ANDROID]
  SUPPORTED_PLATFORM_TAGS = [platforms.ANDROID]

  def __init__(self):
    super(ContribPowerMobile, self).__init__()
    self._browser_package = None

  def CreateStorySet(self, options):
    return ContribPowerMobileTopSitesStorySet()

  def CreateCoreTimelineBasedMeasurementOptions(self):
    options = timeline_based_measurement.Options()
    options.config.chrome_trace_config.SetCategoryFilter(
        ChromeTraceCategoryFilter(''))
    options.config.enable_experimental_system_tracing = True
    options.config.system_trace_config.EnableProfiling(
        '%s*' % self._browser_package, 300)
    options.config.system_trace_config.EnableChrome(
        chrome_trace_config=options.config.chrome_trace_config)
    # options.config.system_trace_config.EnableProcessStats()
    # options.config.system_trace_config.EnablePower()
    # options.config.system_trace_config.EnableFtraceCpu()
    options.config.system_trace_config.EnableFtraceSched()
    return options

  def CustomizeOptions(self, finder_options, possible_browser=None):
    # We are not supposed to overwrite this method (see PerfBenchmark) but it is
    # the only way to get to the possible_browser in time to set the parameters
    # for stack sampling.
    super(ContribPowerMobile, self).CustomizeOptions(finder_options,
                                                     possible_browser)
    self._browser_package = possible_browser.browser_package

  @classmethod
  def Name(cls):
    return 'contrib.power.mobile'
