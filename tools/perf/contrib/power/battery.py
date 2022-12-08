# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import platforms
import contrib.power.stories as stories
from contrib.power.power_perf_benchmark_base import PowerPerfBenchmarkBase
from telemetry import benchmark
from telemetry import story


@benchmark.Info(emails=['chrometto-team@google.com'])
class ContribPowerBattery(PowerPerfBenchmarkBase):

  SUPPORTED_PLATFORMS = [story.expectations.ALL_ANDROID]
  SUPPORTED_PLATFORM_TAGS = [platforms.ANDROID]

  def CreateStorySet(self, options):
    del options  # unused
    return stories.GetAllMobileSystemHealthStories()

  def GetTraceConfig(self, browser_package):
    return """
      buffers: {
          size_kb: 262144
          fill_policy: DISCARD
      }
      data_sources: {
          config {
              name: "android.power"
              android_power_config {
                  battery_poll_ms: 1000
                  battery_counters: BATTERY_COUNTER_CAPACITY_PERCENT
                  battery_counters: BATTERY_COUNTER_CHARGE
                  battery_counters: BATTERY_COUNTER_CURRENT
                  collect_power_rails: true
              }
          }
      }
      write_into_file: true
    """

  @classmethod
  def Name(cls):
    return 'contrib.power.battery'
