# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import sys
from core import perf_benchmark
from core import platforms as core_platforms

import page_sets
from page_sets.system_health import platforms
from telemetry import benchmark
from telemetry import story as story_module
from telemetry.timeline import chrome_trace_category_filter
from telemetry.web_perf import timeline_based_measurement


RENDERING_BENCHMARK_UMA = [
    'Compositing.Display.DrawToSwapUs',
    'CompositorLatency.TotalLatency',
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
    'Graphics.Smoothness.PercentDroppedFrames.AllAnimations',
    'Graphics.Smoothness.PercentDroppedFrames.AllInteractions',
    'Graphics.Smoothness.PercentDroppedFrames.AllSequences',
    'Graphics.Smoothness.PercentDroppedFrames.MainThread.MainThreadAnimation',
    'Graphics.Smoothness.PercentDroppedFrames.MainThread.RAF',
    'Graphics.Smoothness.PercentDroppedFrames.MainThread.TouchScroll',
    'Graphics.Smoothness.PercentDroppedFrames.MainThread.WheelScroll',
    ('Graphics.Smoothness.PercentDroppedFrames'
     '.CompositorThread.CompositorAnimation'),
    'Graphics.Smoothness.PercentDroppedFrames.CompositorThread.PinchZoom',
    'Graphics.Smoothness.PercentDroppedFrames.CompositorThread.TouchScroll',
    'Graphics.Smoothness.PercentDroppedFrames.CompositorThread.WheelScroll',
    'Graphics.Smoothness.PercentDroppedFrames.MainThread.Universal',
    'Graphics.Smoothness.PercentDroppedFrames.CompositorThread.Universal',
    'Graphics.Smoothness.PercentDroppedFrames.SlowerThread.Universal',
    'Graphics.Smoothness.PercentDroppedFrames.ScrollingThread.TouchScroll',
    'Graphics.Smoothness.PercentDroppedFrames.ScrollingThread.WheelScroll',
    'Graphics.Smoothness.MaxPercentDroppedFrames_1sWindow',
    'Graphics.Smoothness.95pctPercentDroppedFrames_1sWindow',
    'Memory.GPU.PeakMemoryUsage.Scroll',
    # TODO(crbug/1175768): Reenable once fixed and not crashing telemetry.
    # 'Memory.GPU.PeakMemoryUsage.PageLoad',
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
    # Supplement the base trace categories with "gpu.memory" which records
    # timings associated with memory ablation experiments.
    category_filter.AddFilterString('gpu.memory')
    category_filter.AddDisabledByDefault(
        'disabled-by-default-histogram_samples')
    options = timeline_based_measurement.Options(category_filter)
    options.config.chrome_trace_config.EnableUMAHistograms(
        *RENDERING_BENCHMARK_UMA)
    options.SetTimelineBasedMetrics([
        'renderingMetric',
        'umaMetric',
        'memoryAblationMetric',
        # Unless --experimentatil-tbmv3-metric flag is used, the following tbmv3
        # metrics do nothing.
        'tbmv3:uma_metrics'
    ])
    return options


@benchmark.Info(emails=['behdadb@chromium.org', 'jonross@chromium.org',
                        'sadrul@chromium.org'],
                documentation_url='https://bit.ly/rendering-benchmarks',
                component='Internals>GPU>Metrics')
class RenderingDesktop(_RenderingBenchmark):
  # TODO(rmhasan): Remove the SUPPORTED_PLATFORMS lists.
  # SUPPORTED_PLATFORMS is deprecated, please put system specifier tags
  # from expectations.config in SUPPORTED_PLATFORM_TAGS.
  SUPPORTED_PLATFORMS = [story_module.expectations.ALL_DESKTOP]
  SUPPORTED_PLATFORM_TAGS = [core_platforms.DESKTOP]
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


@benchmark.Info(emails=['behdadb@chromium.org', 'jonross@chromium.org',
                        'sadrul@chromium.org'],
                documentation_url='https://bit.ly/rendering-benchmarks',
                component='Internals>GPU>Metrics')
class RenderingMobile(_RenderingBenchmark):
  # TODO(rmhasan): Remove the SUPPORTED_PLATFORMS lists.
  # SUPPORTED_PLATFORMS is deprecated, please put system specifier tags
  # from expectations.config in SUPPORTED_PLATFORM_TAGS.
  SUPPORTED_PLATFORMS = [story_module.expectations.ALL_MOBILE]
  SUPPORTED_PLATFORM_TAGS = [core_platforms.MOBILE]
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
    # Force online state for the offline indicator so it doesn't show and affect
    # the benchmarks on bots, which are offline by default.
    options.AppendExtraBrowserArgs(
        '--force-online-connection-state-for-indicator')

  def CreateCoreTimelineBasedMeasurementOptions(self):
    options = super(
        RenderingMobile, self).CreateCoreTimelineBasedMeasurementOptions()
    options.config.enable_platform_display_trace = True
    return options
