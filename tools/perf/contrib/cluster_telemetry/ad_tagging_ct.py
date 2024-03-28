# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from benchmarks import memory
from contrib.cluster_telemetry import ct_benchmarks_util, page_set
from core import perf_benchmark
import py_utils
from telemetry import benchmark
from telemetry.page import cache_temperature
from telemetry.timeline import chrome_trace_category_filter
from telemetry.web_perf import timeline_based_measurement

_NAVIGATION_TIMEOUT = 180
_QUIESCENCE_TIMEOUT = 30


# Benchmark to measure various UMA histograms relevant to AdTagging as well as
# CPU usage on page loads. These measurements will help to determine the
# accuracy of AdTagging.
@benchmark.Info(emails=['alexmt@chromium.org', 'johnidel@chromium.org'],
                component='UI>Browser>AdFilter')
class AdTaggingClusterTelemetry(perf_benchmark.PerfBenchmark):
  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    ct_benchmarks_util.AddBenchmarkCommandLineArgs(parser)
    parser.add_argument(
        '--additional-histograms',
        help='Comma-separated list of additional UMA histograms to record.')
    parser.add_argument('--verbose-cpu-metrics',
                        action='store_true',
                        help='Enables non-UMA CPU metrics.')
    parser.add_argument('--verbose-memory-metrics',
                        action='store_true',
                        help='Enables non-UMA memory metrics.')

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    ct_benchmarks_util.ValidateCommandLineArgs(parser, args)
    cls.additional_histograms = []
    if args.additional_histograms is not None:
      cls.additional_histograms = args.additional_histograms.split(',')
    cls.enable_limited_cpu_time_metric = args.verbose_cpu_metrics
    cls.enable_memory_metric = args.verbose_memory_metrics

  def GetExtraOutDirectories(self):
    # The indexed filter list does not end up in a build directory on cluster
    # telemetry; so we add its location here.
    return ['/b/s/w/ir/out/telemetry_isolates']

  def SetExtraBrowserOptions(self, options):
    if self.enable_memory_metric:
      memory.SetExtraBrowserOptionsForMemoryMeasurement(options)

  def CreateCoreTimelineBasedMeasurementOptions(self):
    category_filter = chrome_trace_category_filter.CreateLowOverheadFilter()
    if self.enable_memory_metric:
      tbm_options = memory.CreateCoreTimelineBasedMemoryMeasurementOptions()

      # The memory options only include the filters needed for memory
      # measurement. We reintroduce the filters required for other metrics.
      tbm_options.ExtendTraceCategoryFilter(
          category_filter.filter_string.split(','))
    else:
      tbm_options = timeline_based_measurement.Options(category_filter)

    uma_histograms = [
        'PageLoad.Clients.Ads.AllPages.NonAdNetworkBytes',
        'PageLoad.Clients.Ads.AllPages.PercentNetworkBytesAds',
        'PageLoad.Clients.Ads.Bytes.AdFrames.Aggregate.Total2',
        'PageLoad.Clients.Ads.Cpu.AdFrames.Aggregate.TotalUsage2',
        'PageLoad.Clients.Ads.Cpu.FullPage.TotalUsage2',
        'PageLoad.Clients.Ads.FrameCounts.AdFrames.Total',
        'PageLoad.Clients.Ads.Resources.Bytes.Ads2',
        'PageLoad.Cpu.TotalUsage',
        'PageLoad.PaintTiming.NavigationToFirstContentfulPaint',
        'SubresourceFilter.PageLoad.NumSubresourceLoads.MatchedRules',
    ]
    uma_histograms.extend(self.additional_histograms)
    for histogram in uma_histograms:
      tbm_options.config.chrome_trace_config.EnableUMAHistograms(histogram)

    tbm_options.AddTimelineBasedMetric('umaMetric')
    if self.enable_limited_cpu_time_metric:
      tbm_options.AddTimelineBasedMetric('limitedCpuTimeMetric')

    return tbm_options

  def CreateStorySet(self, options):
    enable_memory_metric = self.enable_memory_metric

    def NavigateToPageAndLeavePage(self, action_runner):
      url = self.file_path_url_with_scheme if self.is_file else self.url
      action_runner.Navigate(url,
                             self.script_to_evaluate_on_commit,
                             timeout_in_seconds=_NAVIGATION_TIMEOUT)
      try:
        py_utils.WaitFor(action_runner.tab.HasReachedQuiescence,
                         timeout=_QUIESCENCE_TIMEOUT)
      except py_utils.TimeoutException:
        pass

      # We measure memory after the wait as we want a proxy for page memory.
      if enable_memory_metric:
        action_runner.MeasureMemory(deterministic_mode=True)

      # Navigate away to an untracked page to trigger recording of page load
      # metrics
      action_runner.Navigate('about:blank',
                             self.script_to_evaluate_on_commit,
                             timeout_in_seconds=_NAVIGATION_TIMEOUT)

    return page_set.CTPageSet(
        options.urls_list,
        options.user_agent,
        options.archive_data_file,
        run_navigate_steps_callback=NavigateToPageAndLeavePage,
        cache_temperature=cache_temperature.COLD)

  @classmethod
  def Name(cls):
    return 'ad_tagging.cluster_telemetry'
