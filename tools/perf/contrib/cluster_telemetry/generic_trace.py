# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import defaultdict

from core import path_util
path_util.AddTracingToPath()
from core import perf_benchmark

from contrib.cluster_telemetry import ct_benchmarks_util
from contrib.cluster_telemetry import page_set as ct_page_set
import page_sets
from telemetry import benchmark
from telemetry import timeline
from telemetry.page import legacy_page_test

from tracing.trace_data import trace_data as trace_data_module


class _GenericTraceMeasurement(legacy_page_test.LegacyPageTest):

  def __init__(self, options):
    super(_GenericTraceMeasurement, self).__init__()
    self._trace_categories = ','.join(options.trace_categories)
    trace_names = ','.join(options.trace_names).split(',')
    self._trace_names = [name for name in trace_names if name]

  def WillNavigateToPage(self, page, tab):
    config = timeline.tracing_config.TracingConfig()
    config.enable_chrome_trace = True
    config.chrome_trace_config.category_filter.AddFilterString(
        self._trace_categories)
    tab.browser.platform.tracing_controller.StartTracing(config)

  def ValidateAndMeasurePage(self, page, tab, results):
    with tab.browser.platform.tracing_controller.StopTracing() as trace_builder:
      trace_data = trace_builder.AsData()
    measurements = defaultdict(list)
    duration_events = defaultdict(list)
    duration_measurements = defaultdict(list)
    for trace in trace_data.GetTracesFor(trace_data_module.CHROME_TRACE_PART):
      for event in trace['traceEvents']:
        # We collect data from duration begin, complete, instant and count
        # events. See benchmark documentation for details.
        if event['ph'] not in ('B', 'E', 'X', 'I', 'C'):
          continue
        if self._trace_names and event['name'] not in self._trace_names:
          continue
        event_name = '/'.join([event['cat'], event['name']])
        if event['ph'] == 'B':
          duration_events[event_name].append(int(event['ts']))
        elif event['ph'] == 'E':
          elapsed = int(event['ts']) - duration_events[event_name].pop()
          value_name = '/'.join([event_name, 'elapsed'])
          duration_measurements[value_name].append(elapsed)
        elif event['ph'] == 'X':
          elapsed = int(event['dur'])
          value_name = '/'.join([event_name, 'elapsed'])
          duration_measurements[value_name].append(elapsed)
        for arg_name, arg_value in event.get('args', {}).items():
          if not isinstance(arg_value, int):
            continue
          value_name = '/'.join([event_name, arg_name])
          measurements[value_name].append(arg_value)
    for name, value in measurements.items():
      results.AddMeasurement(name, 'count', value)
    for name, value in duration_measurements.items():
      results.AddMeasurement(name, 'us', value)


class _GenericTraceBenchmark(perf_benchmark.PerfBenchmark):
  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    parser.add_argument('--trace-categories',
                        default=[],
                        action='append',
                        help='Trace categories to enable')
    parser.add_argument(
        '--trace-names',
        default=[],
        action='append',
        help=('Names of trace event to collect '
              'If not specified, all trace events in the enabled '
              'categories will be collected'))

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    if not args.trace_categories:
      parser.error('--trace-categories is required')

  def CreatePageTest(self, options):
    return _GenericTraceMeasurement(options)


@benchmark.Info(emails=['wangxianzhu@chromium.org'],
                documentation_url='https://bit.ly/2DIOVy3')
# For local verification.
class GenericTraceTop25(_GenericTraceBenchmark):
  page_set = page_sets.StaticTop25PageSet

  @classmethod
  def Name(cls):
    return 'generic_trace.top25'


@benchmark.Info(emails=['wangxianzhu@chromium.org'],
                documentation_url='https://bit.ly/2DIOVy3')
class GenericTraceClusterTelemetry(_GenericTraceBenchmark):
  @classmethod
  def Name(cls):
    return 'generic_trace_ct'

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    _GenericTraceBenchmark.AddBenchmarkCommandLineArgs(parser)
    ct_benchmarks_util.AddBenchmarkCommandLineArgs(parser)

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    _GenericTraceBenchmark.ProcessCommandLineArgs(parser, args)
    ct_benchmarks_util.ValidateCommandLineArgs(parser, args)

  def CreateStorySet(self, options):
    return ct_page_set.CTPageSet(
        options.urls_list, options.user_agent, options.archive_data_file)
