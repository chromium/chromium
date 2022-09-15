# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import platforms
from contrib.power.top_sites_story import ContribPowerMobileTopSitesStorySet
from contrib.power.perf_benchmark_with_profiling import PerfBenchmarkWithProfiling
from telemetry import benchmark
from telemetry import story


@benchmark.Info(emails=['chrometto-team@google.com'])
class ContribPowerMobile(PerfBenchmarkWithProfiling):

  SUPPORTED_PLATFORMS = [story.expectations.ALL_ANDROID]
  SUPPORTED_PLATFORM_TAGS = [platforms.ANDROID]

  def CreateStorySet(self, options):
    del options  # unused
    return ContribPowerMobileTopSitesStorySet()

  def GetSamplingFrequencyHz(self):
    return 300

  def CustomizeSystemTraceConfig(self, system_trace_config):
    system_trace_config.EnableFtraceSched()

  @classmethod
  def Name(cls):
    return 'contrib.power.mobile'
