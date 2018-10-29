# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from benchmarks import memory
from core import perf_benchmark
from telemetry import benchmark
from telemetry import story
from telemetry.timeline import chrome_trace_category_filter
from telemetry.timeline import chrome_trace_config
from telemetry.web_perf import timeline_based_measurement
from contrib.vr_benchmarks import vr_browsing_mode_pages
from contrib.vr_benchmarks import webvr_sample_pages
from contrib.vr_benchmarks import webvr_wpr_pages
from contrib.vr_benchmarks import webxr_sample_pages


class _BaseVRBenchmark(perf_benchmark.PerfBenchmark):

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    parser.add_option(
        '--shared-prefs-file',
        help='The path relative to the Chromium source root '
        'to a file containing a JSON list of shared '
        'preference files to edit and how to do so. '
        'See examples in //chrome/android/'
        'shared_preference_files/test/')
    parser.add_option(
        '--disable-screen-reset',
        action='store_true',
        default=False,
        help='Disables turning screen off and on after each story. '
        'This is useful for local testing when turning off the '
        'screen leads to locking the phone, which makes Telemetry '
        'not produce valid results.')
    parser.add_option(
        '--recording-wpr',
        action='store_true',
        default=False,
        help='Modifies benchmark behavior slightly while recording WPR files '
             'for it. This largely boils down to adding waits/sleeps in order '
             'to ensure that enough streaming data is recorded for the '
             'benchmark to run without issues.')


class _BaseWebVRWebXRBenchmark(_BaseVRBenchmark):

  SUPPORTED_PLATFORMS = [story.expectations.ALL_ANDROID]

  def CreateCoreTimelineBasedMeasurementOptions(self):
    memory_categories = ['blink.console', 'disabled-by-default-memory-infra']
    gpu_categories = ['gpu']
    debug_categories = ['toplevel', 'viz']
    category_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter(
        ','.join(['-*'] + memory_categories + gpu_categories
            + debug_categories))
    options = timeline_based_measurement.Options(category_filter)
    options.config.enable_android_graphics_memtrack = True
    options.config.enable_platform_display_trace = True

    options.SetTimelineBasedMetrics(['memoryMetric', 'webvrMetric'])
    options.config.chrome_trace_config.SetMemoryDumpConfig(
        chrome_trace_config.MemoryDumpConfig())
    return options

  @classmethod
  def ShouldAddValue(cls, name, from_first_story_run):
    del from_first_story_run  # unused
    return memory.DefaultShouldAddValueForMemoryMeasurement(name)


class _BaseWebVRBenchmark(_BaseWebVRWebXRBenchmark):

  def SetExtraBrowserOptions(self, options):
    memory.SetExtraBrowserOptionsForMemoryMeasurement(options)
    options.AppendExtraBrowserArgs([
        '--enable-webvr',
    ])


class _BaseWebXRBenchmark(_BaseWebVRWebXRBenchmark):

  def SetExtraBrowserOptions(self, options):
    memory.SetExtraBrowserOptionsForMemoryMeasurement(options)
    options.AppendExtraBrowserArgs([
        '--enable-features=WebXR',
    ])


@benchmark.Info(emails=['bsheedy@chromium.org', 'leilei@chromium.org'])
# pylint: disable=too-many-ancestors
class XrWebVrStatic(_BaseWebVRBenchmark):
  """Measures WebVR performance with synthetic sample pages."""

  def CreateStorySet(self, options):
    return webvr_sample_pages.WebVrSamplePageSet()

  @classmethod
  def Name(cls):
    return 'xr.webvr.static'


@benchmark.Info(emails=['bsheedy@chromium.org', 'tiborg@chromium.org'])
# pylint: disable=too-many-ancestors
class XrWebXrStatic(_BaseWebXRBenchmark):
  """Measures WebXR performance with synthetic sample pages."""

  def CreateStorySet(self, options):
    return webxr_sample_pages.WebXrSamplePageSet()

  @classmethod
  def Name(cls):
    return 'xr.webxr.static'


@benchmark.Info(emails=['bsheedy@chromium.org', 'tiborg@chromium.org'])
# pylint: disable=too-many-ancestors
class XrWebVrWprStatic(_BaseWebVRBenchmark):
  """Measures WebVR performance with WPR copies of live websites."""

  def CreateStorySet(self, options):
    return webvr_wpr_pages.WebVrWprPageSet()

  @classmethod
  def Name(cls):
    return 'xr.webvr.wpr.static'


@benchmark.Info(emails=['bsheedy@chromium.org', 'tiborg@chromium.org'])
# pylint: disable=too-many-ancestors
class XrWebVrLiveStatic(_BaseWebVRBenchmark):
  """Measures WebVR performance with live websites.

  This is a superset of xr.webvr.wpr.static, containing all the pages that it
  uses plus some that we would like to test with WPR, but behave differently
  when using WPR compared to the live version.
  """

  def CreateStorySet(self, options):
    if not hasattr(options, 'use_live_sites') or not options.use_live_sites:
      # We log an error instead of raising an exception here because the
      # Telemetry presubmit unittests fail if we raise.
      logging.error('Running the live sites benchmark without using live '
          'sites. Results will likely be incorrect for some sites.')
    return webvr_wpr_pages.WebVrLivePageSet()

  @classmethod
  def Name(cls):
    return 'xr.webvr.live.static'


class _BaseBrowsingBenchmark(_BaseVRBenchmark):

  SUPPORTED_PLATFORMS = [story.expectations.ALL_ANDROID]

  def CreateTimelineBasedMeasurementOptions(self):
    memory_categories = ['blink.console', 'disabled-by-default-memory-infra']
    gpu_categories = ['gpu']
    debug_categories = ['toplevel', 'viz']
    category_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter(
        ','.join(['-*'] + memory_categories + gpu_categories
            + debug_categories))
    options = timeline_based_measurement.Options(category_filter)
    options.config.enable_android_graphics_memtrack = True
    options.config.enable_platform_display_trace = True
    options.SetTimelineBasedMetrics(['frameCycleDurationMetric',
      'memoryMetric'])
    options.config.chrome_trace_config.SetMemoryDumpConfig(
        chrome_trace_config.MemoryDumpConfig())
    return options

  def SetExtraBrowserOptions(self, options):
    options.clear_sytem_cache_for_browser_and_profile_on_start = True
    options.AppendExtraBrowserArgs([
        '--enable-gpu-benchmarking',
        '--touch-events=enabled',
        '--enable-vr-shell',
    ])


@benchmark.Info(emails=['tiborg@chromium.org'])
class XrBrowsingStatic(_BaseBrowsingBenchmark):
  """Benchmark for testing the VR Browsing Mode performance on sample pages."""

  def CreateStorySet(self, options):
    return vr_browsing_mode_pages.VrBrowsingModePageSet()

  @classmethod
  def Name(cls):
    return 'xr.browsing.static'


@benchmark.Info(emails=['tiborg@chromium.org', 'bsheedy@chromium.org'])
class XrBrowsingWprStatic(_BaseBrowsingBenchmark):
  """Benchmark for testing the VR Browsing Mode performance on WPR pages."""

  def CreateStorySet(self, options):
    return vr_browsing_mode_pages.VrBrowsingModeWprPageSet()

  @classmethod
  def Name(cls):
    return 'xr.browsing.wpr.static'


@benchmark.Info(emails=['tiborg@chromium.org', 'bsheedy@chromium.org'])
class XrBrowsingWprSmoothness(_BaseBrowsingBenchmark):
  """Benchmark for testing VR browser scrolling smoothness and throughput."""

  def CreateCoreTimelineBasedMeasurementOptions(self):
    category_filter = chrome_trace_category_filter.CreateLowOverheadFilter()
    options = timeline_based_measurement.Options(category_filter)
    options.SetTimelineBasedMetrics(['renderingMetric'])
    return options

  def CreateStorySet(self, options):
    return vr_browsing_mode_pages.VrBrowsingModeWprSmoothnessPageSet()

  @classmethod
  def Name(cls):
    return 'xr.browsing.wpr.smoothness'
