# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark
from core import platforms

import page_sets
from page_sets.system_health import story_tags
from telemetry import benchmark
from telemetry import story
from telemetry.web_perf import timeline_based_measurement


@benchmark.Info(
    emails=['chrometto-team@google.com'],
    documentation_url='https://goto.google.com/power-mobile-benchmark')
class PowerMobile(perf_benchmark.PerfBenchmark):
  """A benchmark for power measurements using on-device power monitor (ODPM).
  """

  SUPPORTED_PLATFORMS = [story.expectations.ALL_ANDROID]
  SUPPORTED_PLATFORM_TAGS = [platforms.ANDROID]

  def CreateStorySet(self, options):
    return page_sets.SystemHealthStorySet(platform='mobile',
                                          tag=story_tags.INFINITE_SCROLL)

  def CreateCoreTimelineBasedMeasurementOptions(self):
    options = timeline_based_measurement.Options()
    options.config.enable_experimental_system_tracing = True
    options.config.system_trace_config.EnableChrome(
        chrome_trace_config=options.config.chrome_trace_config)
    options.config.system_trace_config.EnablePower()
    options.config.system_trace_config.EnableFtraceCpu()
    options.config.system_trace_config.EnableFtraceSched()
    options.SetTimelineBasedMetrics(
        ['tbmv3:power_rails_metric', 'tbmv3:power_cpu_estimate'])
    return options

  @classmethod
  def Name(cls):
    return 'power.mobile'
