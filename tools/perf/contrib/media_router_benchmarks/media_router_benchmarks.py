# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from core import path_util
from core import perf_benchmark

from telemetry import benchmark
from telemetry import story
from contrib.media_router_benchmarks import media_router_measurements
from contrib.media_router_benchmarks import media_router_pages


@benchmark.Info(emails=['mfoltz@chromium.org', 'cliffordcheng@chromium.org'],
                component='Internals>Cast')
class MediaRouterCPUMemoryCast(perf_benchmark.PerfBenchmark):
  """Obtains media performance for key user scenarios on desktop."""

  SUPPORTED_PLATFORMS = [story.expectations.ALL_DESKTOP]
  options = {'pageset_repeat': 1}
  page_set = media_router_pages.MediaRouterCPUMemoryPageSet

  def SetExtraBrowserOptions(self, options):
    options.clear_sytem_cache_for_browser_and_profile_on_start = True
    # This flag is required to enable the communication between the page and
    # the test extension.
    options.disable_background_networking = False

    options.AppendExtraBrowserArgs([
        '--load-extension=' + ','.join(
            [os.path.join(path_util.GetChromiumSrcDir(), 'out',
             'Release', 'mr_extension'),
             os.path.join(path_util.GetChromiumSrcDir(), 'out',
             'Release', 'media_router', 'telemetry_extension')]),
        '--disable-features=ViewsCastDialog',
        '--whitelisted-extension-id=enhhojjnijigcajfphajepfemndkmdlo',
        '--media-router=1',
        '--enable-stats-collection-bindings'
    ])

  def CreatePageTest(self, options):
    return media_router_measurements.MediaRouterCPUMemoryTest()

  @classmethod
  def Name(cls):
    return 'media_router.cpu_memory'


@benchmark.Info(emails=['mfoltz@chromium.org', 'cliffordcheng@chromium.org'],
                component='Internals>Cast')
class NoMediaRouterCPUMemory(perf_benchmark.PerfBenchmark):
  """Benchmark for CPU and memory usage without Media Router."""

  SUPPORTED_PLATFORMS = [story.expectations.ALL_DESKTOP]
  options = {'pageset_repeat': 1}
  page_set = media_router_pages.CPUMemoryPageSet

  def SetExtraBrowserOptions(self, options):
    options.clear_sytem_cache_for_browser_and_profile_on_start = True
    # This flag is required to enable the communication between the page and
    # the test extension.
    options.disable_background_networking = False
    options.AppendExtraBrowserArgs([
        '--load-extension=' +
            os.path.join(path_util.GetChromiumSrcDir(), 'out',
            'Release', 'media_router', 'telemetry_extension'),
        '--disable-features=ViewsCastDialog',
        '--media-router=0',
        '--enable-stats-collection-bindings'
    ])

  def CreatePageTest(self, options):
    return media_router_measurements.MediaRouterCPUMemoryTest()

  @classmethod
  def Name(cls):
    return 'media_router.cpu_memory.no_media_router'
