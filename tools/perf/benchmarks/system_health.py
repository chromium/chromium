# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging

from benchmarks import loading_metrics_category

from core import perf_benchmark
from core import platforms

from telemetry import benchmark
from telemetry import story
from telemetry.timeline import chrome_trace_category_filter
from telemetry.timeline import chrome_trace_config
from telemetry.web_perf import timeline_based_measurement
import page_sets


SYSTEM_HEALTH_BENCHMARK_UMA = [
    'EventLatency.FirstGestureScrollUpdate.TotalLatency2',
    'EventLatency.GestureScrollUpdate.TotalLatency2',
    'Graphics.Smoothness.PercentDroppedFrames3.AllSequences',
    'Memory.GPU.PeakMemoryUsage2.Scroll',
    'Memory.GPU.PeakMemoryUsage2.PageLoad',
    'Memory.Experimental.Renderer2.Small.Malloc.BRPQuarantined',
]


class _CommonSystemHealthBenchmark(perf_benchmark.PerfBenchmark):
  """Chrome Common System Health Benchmark.

  This test suite contains system health benchmarks that can be collected
  together due to the low overhead of the tracing agents required. If a
  benchmark does have significant overhead, it should either:

    1) Be rearchitected such that it doesn't. This is the most preferred option.
    2) Be run in a separate test suite (e.g. memory).

  https://goo.gl/Jek2NL.
  """

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    parser.add_argument(
        '--allow-software-compositing',
        action='store_true',
        help='If set, allows the benchmark to run with software compositing.')

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    cls.allow_software_compositing = args.allow_software_compositing

  def CreateCoreTimelineBasedMeasurementOptions(self):
    cat_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter(
        filter_string='rail,toplevel,uma')
    cat_filter.AddIncludedCategory('accessibility')
    # Needed for the metric reported by page.
    cat_filter.AddIncludedCategory('blink.user_timing')
    # Needed for blinkResourceMetric,
    cat_filter.AddIncludedCategory('blink.resource')
    # Needed for the console error metric.
    cat_filter.AddIncludedCategory('v8.console')

    options = timeline_based_measurement.Options(cat_filter)
    options.config.enable_chrome_trace = True
    options.config.enable_cpu_trace = True
    options.config.chrome_trace_config.EnableUMAHistograms(
        *SYSTEM_HEALTH_BENCHMARK_UMA)
    options.SetTimelineBasedMetrics([
        'accessibilityMetric',
        'blinkResourceMetric',
        'consoleErrorMetric',
        'cpuTimeMetric',
        'limitedCpuTimeMetric',
        'reportedByPageMetric',
        'tracingMetric',
        'umaMetric',
        # Unless --experimentatil-tbmv3-metric flag is used, the following tbmv3
        # metrics do nothing.
        'tbmv3:accessibility_metric',
        'tbmv3:cpu_time_metric',
    ])
    loading_metrics_category.AugmentOptionsForLoadingMetrics(options)
    # The EQT metric depends on the same categories as the loading metric.
    options.AddTimelineBasedMetric('expectedQueueingTimeMetric')
    return options

  def SetExtraBrowserOptions(self, options):
    # Using the software fallback can skew the rendering related metrics. So
    # disable that (unless explicitly run with --allow-software-compositing).
    #
    # TODO(jonross): Catapult's record_wpr.py calls SetExtraBrowserOptions
    # before calling ProcessCommandLineArgs. This will crash attempting to
    # record new system health benchmarks. We do not want to support software
    # compositing for recording, so for now we will just check for the existence
    # the flag. We will review updating Catapult at a later point.
    if (hasattr(self, 'allow_software_compositing')
        and self.allow_software_compositing) or self.NeedsSoftwareCompositing():
      logging.warning('Allowing software compositing. Some of the reported '
                      'metrics will have unreliable values.')
    else:
      options.AppendExtraBrowserArgs('--disable-software-compositing-fallback')

  def CreateStorySet(self, options):
    return page_sets.SystemHealthStorySet(platform=self.PLATFORM)


@benchmark.Info(emails=['kouhei@chromium.org'],
                component='Speed>Metrics>SystemHealthRegressions',
                documentation_url='https://bit.ly/system-health-benchmarks')
class DesktopCommonSystemHealth(_CommonSystemHealthBenchmark):
  """Desktop Chrome Energy System Health Benchmark."""
  PLATFORM = 'desktop'
  # TODO(johnchen): Remove either the SUPPORTED_PLATFORMS or
  # SUPPORTED_PLATFORMS_TAGS lists. Only one is necessary.
  SUPPORTED_PLATFORM_TAGS = [platforms.DESKTOP]
  SUPPORTED_PLATFORMS = [story.expectations.ALL_DESKTOP]

  @classmethod
  def Name(cls):
    return 'system_health.common_desktop'

  def CreateCoreTimelineBasedMeasurementOptions(self):
    options = super(DesktopCommonSystemHealth,
                    self).CreateCoreTimelineBasedMeasurementOptions()
    options.config.chrome_trace_config.SetTraceBufferSizeInKb(400 * 1024)
    return options


@benchmark.Info(emails=['kouhei@chromium.org'],
                component='Speed>Metrics>SystemHealthRegressions',
                documentation_url='https://bit.ly/system-health-benchmarks')
class MobileCommonSystemHealth(_CommonSystemHealthBenchmark):
  """Mobile Chrome Energy System Health Benchmark."""
  PLATFORM = 'mobile'
  # TODO(johnchen): Remove either the SUPPORTED_PLATFORMS or
  # SUPPORTED_PLATFORMS_TAGS lists. Only one is necessary.
  SUPPORTED_PLATFORM_TAGS = [platforms.MOBILE]
  SUPPORTED_PLATFORMS = [story.expectations.ALL_MOBILE]

  @classmethod
  def Name(cls):
    return 'system_health.common_mobile'

  def SetExtraBrowserOptions(self, options):
    super(MobileCommonSystemHealth, self).SetExtraBrowserOptions(options)
    # Force online state for the offline indicator so it doesn't show and affect
    # the benchmarks on bots, which are offline by default.
    options.AppendExtraBrowserArgs(
        '--force-online-connection-state-for-indicator')


class _MemorySystemHealthBenchmark(perf_benchmark.PerfBenchmark):
  """Chrome Memory System Health Benchmark.

  This test suite is run separately from the common one due to the high overhead
  of memory tracing.

  https://goo.gl/Jek2NL.
  """
  options = {'pageset_repeat': 3}

  def CreateCoreTimelineBasedMeasurementOptions(self):
    cat_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter(
        filter_string='-*,disabled-by-default-memory-infra')
    # Needed for the console error metric.
    cat_filter.AddIncludedCategory('v8.console')
    options = timeline_based_measurement.Options(cat_filter)
    options.config.enable_android_graphics_memtrack = True
    options.SetTimelineBasedMetrics([
      'consoleErrorMetric',
      'memoryMetric'
    ])
    # Setting an empty memory dump config disables periodic dumps.
    options.config.chrome_trace_config.SetMemoryDumpConfig(
        chrome_trace_config.MemoryDumpConfig())
    return options

  def CreateStorySet(self, options):
    return page_sets.SystemHealthStorySet(platform=self.PLATFORM,
                                          take_memory_measurement=True)


MEMORY_DEBUGGING_BLURB = "See https://bit.ly/2CpMhze for more information" \
                         " on debugging memory metrics."


@benchmark.Info(emails=['pasko@chromium.org', 'lizeb@chromium.org'],
                documentation_url='https://bit.ly/system-health-benchmarks',
                info_blurb=MEMORY_DEBUGGING_BLURB)
class DesktopMemorySystemHealth(_MemorySystemHealthBenchmark):
  """Desktop Chrome Memory System Health Benchmark."""
  PLATFORM = 'desktop'
  # TODO(johnchen): Remove either the SUPPORTED_PLATFORMS or
  # SUPPORTED_PLATFORMS_TAGS lists. Only one is necessary.
  SUPPORTED_PLATFORM_TAGS = [platforms.DESKTOP]
  SUPPORTED_PLATFORMS = [story.expectations.ALL_DESKTOP]

  @classmethod
  def Name(cls):
    return 'system_health.memory_desktop'


@benchmark.Info(emails=['pasko@chromium.org', 'lizeb@chromium.org'],
                documentation_url='https://bit.ly/system-health-benchmarks',
                info_blurb=MEMORY_DEBUGGING_BLURB)
class MobileMemorySystemHealth(_MemorySystemHealthBenchmark):
  """Mobile Chrome Memory System Health Benchmark."""
  PLATFORM = 'mobile'
  # TODO(johnchen): Remove either the SUPPORTED_PLATFORMS or
  # SUPPORTED_PLATFORMS_TAGS lists. Only one is necessary.
  SUPPORTED_PLATFORM_TAGS = [platforms.MOBILE]
  SUPPORTED_PLATFORMS = [story.expectations.ALL_MOBILE]

  def SetExtraBrowserOptions(self, options):
    # Just before we measure memory we flush the system caches
    # unfortunately this doesn't immediately take effect, instead
    # the next story run is effected. Due to this the first story run
    # has anomalous results. This option causes us to flush caches
    # each time before Chrome starts so we effect even the first story
    # - avoiding the bug.
    options.flush_os_page_caches_on_start = True
    # Force online state for the offline indicator so it doesn't show and affect
    # the benchmarks on bots, which are offline by default.
    options.AppendExtraBrowserArgs(
        '--force-online-connection-state-for-indicator')

  @classmethod
  def Name(cls):
    return 'system_health.memory_mobile'


@benchmark.Info(emails=['oksamyt@chromium.org', 'torne@chromium.org'],
                component='Mobile>WebView>Perf')
class WebviewStartupSystemHealthBenchmark(perf_benchmark.PerfBenchmark):
  """Webview startup time benchmark

  Benchmark that measures how long WebView takes to start up
  and load a blank page.
  """
  options = {'pageset_repeat': 20}
  # TODO(johnchen): Remove either the SUPPORTED_PLATFORMS or
  # SUPPORTED_PLATFORMS_TAGS lists. Only one is necessary.
  SUPPORTED_PLATFORM_TAGS = [platforms.ANDROID_WEBVIEW]
  SUPPORTED_PLATFORMS = [story.expectations.ANDROID_WEBVIEW]

  def CreateStorySet(self, options):
    return page_sets.SystemHealthBlankStorySet()

  def CreateCoreTimelineBasedMeasurementOptions(self):
    options = timeline_based_measurement.Options()
    options.SetTimelineBasedMetrics(['webviewStartupMetric'])
    options.config.enable_atrace_trace = True
    # TODO(crbug.com/40109346): Recording a Chrome trace at the same time as
    # atrace causes events to stack incorrectly. Fix this by recording a
    # system+Chrome trace via system perfetto on the device instead.
    options.config.enable_chrome_trace = False
    options.config.atrace_config.app_name = 'org.chromium.webview_shell'
    return options

  @classmethod
  def Name(cls):
    return 'system_health.webview_startup'
