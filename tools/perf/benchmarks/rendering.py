# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import sys
from core import perf_benchmark

import page_sets
from page_sets.system_health import platforms
from telemetry import benchmark
from telemetry import story as story_module
from telemetry.timeline import chrome_trace_category_filter
from telemetry.web_perf import timeline_based_measurement


RENDERING_BENCHMARK_UMA = [
    'Event.Latency.ScrollBegin.Touch.TimeToScrollUpdateSwapBegin4',
    'Event.Latency.ScrollUpdate.Touch.TimeToScrollUpdateSwapBegin4',
    'Event.Latency.ScrollBegin.Wheel.TimeToScrollUpdateSwapBegin4',
    'Event.Latency.ScrollUpdate.Wheel.TimeToScrollUpdateSwapBegin4',
    'Graphics.Smoothness.Checkerboarding.CompositorAnimation',
    'Graphics.Smoothness.Checkerboarding.MainThreadAnimation',
    'Graphics.Smoothness.Checkerboarding.PinchZoom',
    'Graphics.Smoothness.Checkerboarding.RAF',
    'Graphics.Smoothness.Checkerboarding.TouchScroll',
    'Graphics.Smoothness.Checkerboarding.Video',
    'Graphics.Smoothness.Checkerboarding.WheelScroll',
    'Graphics.Smoothness.Throughput.MainThread.MainThreadAnimation',
    'Graphics.Smoothness.Throughput.MainThread.PinchZoom',
    'Graphics.Smoothness.Throughput.MainThread.RAF',
    'Graphics.Smoothness.Throughput.MainThread.TouchScroll',
    'Graphics.Smoothness.Throughput.MainThread.WheelScroll',
    'Graphics.Smoothness.Throughput.CompositorThread.CompositorAnimation',
    'Graphics.Smoothness.Throughput.CompositorThread.PinchZoom',
    'Graphics.Smoothness.Throughput.CompositorThread.TouchScroll',
    'Graphics.Smoothness.Throughput.CompositorThread.WheelScroll',
    'Memory.GPU.PeakMemoryUsage.Scroll',
]


class _RenderingBenchmark(perf_benchmark.PerfBenchmark):
  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    parser.add_option('--scroll-forever', action='store_true',
                      help='If set, continuously scroll up and down forever. '
                           'This is useful for analysing scrolling behaviour '
                           'with tools such as perf.')

  def CreateStorySet(self, options):
    return page_sets.RenderingStorySet(platform=self.PLATFORM_NAME)

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-gpu-benchmarking')
    options.AppendExtraBrowserArgs('--touch-events=enabled')
    options.AppendExtraBrowserArgs('--disable-software-compositing-fallback')

  def CreateCoreTimelineBasedMeasurementOptions(self):
    category_filter = chrome_trace_category_filter.CreateLowOverheadFilter()
    options = timeline_based_measurement.Options(category_filter)
    options.config.chrome_trace_config.EnableUMAHistograms(
        *RENDERING_BENCHMARK_UMA)
    options.SetTimelineBasedMetrics(['renderingMetric', 'umaMetric'])
    return options


@benchmark.Info(emails=['sadrul@chromium.org', 'vmiura@chromium.org'],
                documentation_url='https://bit.ly/rendering-benchmarks',
                component='Internals>GPU>Metrics')
class RenderingDesktop(_RenderingBenchmark):
  SUPPORTED_PLATFORMS = [story_module.expectations.ALL_DESKTOP]
  PLATFORM_NAME = platforms.DESKTOP

  @classmethod
  def Name(cls):
    return 'rendering.desktop'

  def SetExtraBrowserOptions(self, options):
    super(RenderingDesktop, self).SetExtraBrowserOptions(options)
    # The feature below is only needed for macOS.
    # We found that the normal priorities used for mac is resulting into
    # unreliable values for avg_fps and frame_times. Increasing the priority
    # and using it in telemetry tests can help with more accurate values.
    # crbug.com/970607
    if sys.platform == 'darwin':
      options.AppendExtraBrowserArgs(
          '--use-gpu-high-thread-priority-for-perf-tests')


@benchmark.Info(emails=['sadrul@chromium.org', 'vmiura@chromium.org'],
                documentation_url='https://bit.ly/rendering-benchmarks',
                component='Internals>GPU>Metrics')
class RenderingMobile(_RenderingBenchmark):
  SUPPORTED_PLATFORMS = [story_module.expectations.ALL_MOBILE]
  PLATFORM_NAME = platforms.MOBILE

  @classmethod
  def Name(cls):
    return 'rendering.mobile'

  def SetExtraBrowserOptions(self, options):
    super(RenderingMobile, self).SetExtraBrowserOptions(options)
    # Disable locking the controls as visible for a minimum duration. This
    # allows controls to unlock after page load, rather than in the middle of a
    # story.
    options.AppendExtraBrowserArgs('--disable-minimum-show-duration')

  def CreateCoreTimelineBasedMeasurementOptions(self):
    options = super(
        RenderingMobile, self).CreateCoreTimelineBasedMeasurementOptions()
    options.config.enable_platform_display_trace = True
    return options
