# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging
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
    'EventLatency.FirstGestureScrollUpdate.TotalLatency2',
    'EventLatency.GestureScrollUpdate.Touchscreen.TotalLatency',
    'EventLatency.GestureScrollUpdate.TotalLatency2',
    'Graphics.Smoothness.Checkerboarding4.AllAnimations',
    'Graphics.Smoothness.Checkerboarding4.AllInteractions',
    'Graphics.Smoothness.Checkerboarding4.AllSequences',
    'Graphics.Smoothness.CheckerboardingNeedRaster4.AllSequences',
    'Graphics.Smoothness.CheckerboardingNeedRecord4.AllSequences',
    'Graphics.Smoothness.Jank3.AllAnimations',
    'Graphics.Smoothness.Jank3.AllInteractions',
    'Graphics.Smoothness.Jank3.AllSequences',
    'Graphics.Smoothness.PercentDroppedFrames3.AllAnimations',
    'Graphics.Smoothness.PercentDroppedFrames3.AllInteractions',
    'Graphics.Smoothness.PercentDroppedFrames3.AllSequences',
    'Memory.GPU.PeakMemoryUsage2.Scroll',
    'Memory.GPU.PeakMemoryUsage2.PageLoad',
    'Event.Jank.PredictorJankyFramePercentage2',
    'Event.ScrollJank.DelayedFramesPercentage.FixedWindow',
    'Event.ScrollJank.DelayedFramesPercentage.PerScroll',
    'Event.ScrollJank.MissedVsyncsSum.FixedWindow',
    'Event.ScrollJank.MissedVsyncsSum.PerScroll',
    'Event.ScrollJank.MissedVsyncsPercentage.FixedWindow',
    'Event.ScrollJank.MissedVsyncsPercentage.PerScroll',
]


class _RenderingBenchmark(perf_benchmark.PerfBenchmark):
  # TODO(crbug.com/40764818): Capturing video is causing long cycle time and timeout
  # on some Pixel devices. Disabling this option until the issue can be fixed.
  #options = {
  #    'capture_screen_video': True
  #}

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    parser.add_argument(
        '--scroll-forever',
        action='store_true',
        help=('If set, continuously scroll up and down forever. '
              'This is useful for analysing scrolling behaviour '
              'with tools such as perf.'))
    parser.add_argument(
        '--allow-software-compositing',
        action='store_true',
        help=('If set, allows the benchmark to run with software '
              'compositing.'))
    parser.add_argument(
        '--extra-uma-metrics',
        help=('Comma separated list of additional UMA metrics to '
              'include in result output. Note that histogram buckets '
              'in telemetry report may not match buckets from UMA.'))

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    cls.allow_software_compositing = args.allow_software_compositing
    cls.uma_metrics = RENDERING_BENCHMARK_UMA
    if args.extra_uma_metrics:
      cls.uma_metrics += args.extra_uma_metrics.split(',')

  def CreateStorySet(self, options):
    return page_sets.RenderingStorySet(platform=self.PLATFORM_NAME)

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-gpu-benchmarking')
    options.AppendExtraBrowserArgs('--touch-events=enabled')
    # TODO(jonross): Catapult's record_wpr.py calls SetExtraBrowserOptions
    # before calling ProcessCommandLineArgs. This will crash attempting to
    # record new rendering benchmarks. We do not want to support software
    # compositing for recording, so for now we will just check for the existence
    # the flag. We will review updating Catapult at a later point.
    if (hasattr(self, 'allow_software_compositing')
        and self.allow_software_compositing) or self.NeedsSoftwareCompositing():
      logging.warning('Allowing software compositing. Some of the reported '
                      'metrics will have unreliable values.')
    else:
      options.AppendExtraBrowserArgs('--disable-software-compositing-fallback')

  def CreateCoreTimelineBasedMeasurementOptions(self):
    category_filter = chrome_trace_category_filter.CreateLowOverheadFilter()
    category_filter.AddDisabledByDefault(
        'disabled-by-default-histogram_samples')
    options = timeline_based_measurement.Options(category_filter)
    options.config.chrome_trace_config.EnableUMAHistograms(*self.uma_metrics)
    options.SetTimelineBasedMetrics([
        'renderingMetric',
        'umaMetric',
        # Unless --experimentatil-tbmv3-metric flag is used, the following tbmv3
        # metrics do nothing.
        'tbmv3:uma_metrics'
    ])
    return options


@benchmark.Info(
    emails=['jonross@chromium.org', 'chrome-gpu-metrics@google.com'],
    documentation_url='https://bit.ly/rendering-benchmarks',
    component='Internals>GPU>Metrics')
class RenderingDesktop(_RenderingBenchmark):
  # TODO(johnchen): Remove either the SUPPORTED_PLATFORMS or
  # SUPPORTED_PLATFORMS_TAGS lists. Only one is necessary.
  SUPPORTED_PLATFORMS = [story_module.expectations.ALL_DESKTOP]
  SUPPORTED_PLATFORM_TAGS = [core_platforms.DESKTOP]
  PLATFORM_NAME = platforms.DESKTOP

  @classmethod
  def Name(cls):
    return 'rendering.desktop'

  def SetExtraBrowserOptions(self, options):
    super(RenderingDesktop, self).SetExtraBrowserOptions(options)
    if sys.platform == 'darwin':
      # Mac bots without a physical display fallbacks to SRGB. This flag forces
      # them to use a color profile (P3), which matches the usual color profile
      # on Mac monitors and changes the cost of some overlay operations to match
      # real conditions more closely.
      options.AppendExtraBrowserArgs('--force-color-profile=display-p3-d65')


@benchmark.Info(
    emails=['jonross@chromium.org', 'chrome-gpu-metrics@google.com'],
    documentation_url='https://bit.ly/rendering-benchmarks',
    component='Internals>GPU>Metrics')
class RenderingDesktopNoTracing(RenderingDesktop):
  @classmethod
  def Name(cls):
    return 'rendering.desktop.notracing'

  def CreateStorySet(self, options):
    os_name = None
    # Archive Validation does not perform OS validation
    if hasattr(options, 'os_name'):
      os_name = options.os_name
    return page_sets.RenderingStorySet(platform=self.PLATFORM_NAME,
                                       disable_tracing=True,
                                       os_name=os_name)

  def CreateCoreTimelineBasedMeasurementOptions(self):
    options = timeline_based_measurement.Options()
    options.config.enable_chrome_trace = False
    options.config.enable_platform_display_trace = False
    return options


@benchmark.Info(
    emails=['jonross@chromium.org', 'chrome-gpu-metrics@google.com'],
    documentation_url='https://bit.ly/rendering-benchmarks',
    component='Internals>GPU>Metrics')
class RenderingMobile(_RenderingBenchmark):
  # TODO(johnchen): Remove either the SUPPORTED_PLATFORMS or
  # SUPPORTED_PLATFORMS_TAGS lists. Only one is necessary.
  SUPPORTED_PLATFORMS = [
      story_module.expectations.ALL_MOBILE,
      story_module.expectations.FUCHSIA_ASTRO,
      story_module.expectations.FUCHSIA_SHERLOCK
  ]
  SUPPORTED_PLATFORM_TAGS = [
      core_platforms.MOBILE, core_platforms.FUCHSIA_ASTRO,
      core_platforms.FUCHSIA_SHERLOCK
  ]
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


@benchmark.Info(
    emails=['jonross@chromium.org', 'chrome-gpu-metrics@google.com'],
    documentation_url='https://bit.ly/rendering-benchmarks',
    component='Internals>GPU>Metrics')
class RenderingMobileNoTracing(RenderingMobile):
  @classmethod
  def Name(cls):
    return 'rendering.mobile.notracing'

  def CreateStorySet(self, options):
    return page_sets.RenderingStorySet(platform=self.PLATFORM_NAME,
                                       disable_tracing=True)

  def CreateCoreTimelineBasedMeasurementOptions(self):
    options = timeline_based_measurement.Options()
    options.config.enable_chrome_trace = False
    options.config.enable_platform_display_trace = False
    return options
