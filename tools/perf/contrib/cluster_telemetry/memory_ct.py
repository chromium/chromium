# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from benchmarks import memory
from core import perf_benchmark

from contrib.cluster_telemetry import ct_benchmarks_util
from contrib.cluster_telemetry import page_set

from telemetry.page import traffic_setting


class MemoryClusterTelemetry(perf_benchmark.PerfBenchmark):

  options = {'upload_results': True}

  _ALL_NET_CONFIGS = list(traffic_setting.NETWORK_CONFIGS.keys())
  enable_heap_profiling = True

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    super(MemoryClusterTelemetry, cls).AddBenchmarkCommandLineArgs(parser)
    ct_benchmarks_util.AddBenchmarkCommandLineArgs(parser)
    parser.add_argument('--wait-time',
                        type=int,
                        default=60,
                        help=('Number of seconds to wait for after navigation '
                              'and before taking memory dump.'))
    parser.add_argument(
        '--traffic-setting',
        choices=cls._ALL_NET_CONFIGS,
        default=traffic_setting.REGULAR_4G,
        help='Traffic condition (string). Default to "%(default)s".')
    parser.add_argument(
        '--disable-heap-profiling',
        action='store_true',
        help=('Disable heap profiling to reduce perf overhead. Notes that this '
              'makes test more realistic but give less accurate memory '
              'metrics'))

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    super(MemoryClusterTelemetry, cls).ProcessCommandLineArgs(parser, args)
    cls.enable_heap_profiling = not args.disable_heap_profiling

  def CreateCoreTimelineBasedMeasurementOptions(self):
    return memory.CreateCoreTimelineBasedMemoryMeasurementOptions()

  def SetExtraBrowserOptions(self, options):
    memory.SetExtraBrowserOptionsForMemoryMeasurement(options)
    if self.enable_heap_profiling:
      options.AppendExtraBrowserArgs([
          '--memlog=all --memlog-stack-mode=pseudo',
      ])

  def CreateStorySet(self, options):
    def WaitAndMeasureMemory(action_runner):
      action_runner.Wait(options.wait_time)
      action_runner.MeasureMemory(deterministic_mode=True)

    return page_set.CTPageSet(
      options.urls_list, options.user_agent, options.archive_data_file,
      traffic_setting=options.traffic_setting,
      run_page_interaction_callback=WaitAndMeasureMemory)

  @classmethod
  def Name(cls):
    return 'memory.cluster_telemetry'
