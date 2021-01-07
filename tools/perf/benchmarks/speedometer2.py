# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Apple's Speedometer 2 performance benchmark.
"""

import os
import re

from benchmarks import press

from core import path_util

from telemetry import benchmark
from telemetry import story
from telemetry.timeline import chrome_trace_category_filter
from telemetry.web_perf import timeline_based_measurement

from page_sets import speedometer2_pages

_SPEEDOMETER_DIR = os.path.join(path_util.GetChromiumSrcDir(),
    'third_party', 'blink', 'perf_tests', 'speedometer')

@benchmark.Info(emails=['hablich@chromium.org'],
                component='Blink')
class Speedometer2(press._PressBenchmark): # pylint: disable=protected-access
  """Speedometer2 Benchmark.

  Runs all the speedometer 2 suites by default. Add --suite=<regex> to filter
  out suites, and only run suites whose names are matched by the regular
  expression provided.
  """

  enable_smoke_test_mode = False
  enable_rcs = False

  @classmethod
  def Name(cls):
    return 'speedometer2'

  def CreateStorySet(self, options):
    should_filter_suites = bool(options.suite)
    filtered_suite_names = map(
        speedometer2_pages.Speedometer2Story.GetFullSuiteName,
            speedometer2_pages.Speedometer2Story.GetSuites(options.suite))

    ps = story.StorySet(base_dir=_SPEEDOMETER_DIR)
    ps.AddStory(speedometer2_pages.Speedometer2Story(ps, should_filter_suites,
        filtered_suite_names, self.enable_smoke_test_mode))
    return ps

  def CreateCoreTimelineBasedMeasurementOptions(self):
    if not self.enable_rcs:
      return timeline_based_measurement.Options()

    cat_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter()

    # "blink.console" is used for marking ranges in
    # cache_temperature.MarkTelemetryInternal.
    cat_filter.AddIncludedCategory('blink.console')

    # "toplevel" category is used to capture TaskQueueManager events.
    cat_filter.AddIncludedCategory('toplevel')

    # V8 needed categories
    cat_filter.AddIncludedCategory('v8')
    cat_filter.AddDisabledByDefault('disabled-by-default-v8.runtime_stats')

    tbm_options = timeline_based_measurement.Options(overhead_level=cat_filter)
    tbm_options.SetTimelineBasedMetrics(['runtimeStatsTotalMetric'])
    return tbm_options

  def SetExtraBrowserOptions(self, options):
    if self.enable_rcs:
      options.AppendExtraBrowserArgs(
          '--enable-blink-features=BlinkRuntimeCallStats')

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    parser.add_option('--suite', type="string",
                      help="Only runs suites that match regex provided")
    parser.add_option('--enable-rcs',
                      action="store_true",
                      help="Enables runtime call stats")

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    if args.suite:
      try:
        if not speedometer2_pages.Speedometer2Story.GetSuites(args.suite):
          raise parser.error('--suite: No matches.')
      except re.error:
        raise parser.error('--suite: Invalid regex.')
    if args.enable_rcs:
      cls.enable_rcs = True


@benchmark.Info(emails=['hablich@chromium.org'],
                component='Blink')
class V8Speedometer2Future(Speedometer2):
  """Speedometer2 benchmark with the V8 flag --future.

  Shows the performance of upcoming V8 VM features.
  """

  @classmethod
  def Name(cls):
    return 'speedometer2-future'

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-features=V8VmFuture')


@benchmark.Info(emails=['tmrts@chromium.org'], component='Blink')
class Speedometer2PCScan(Speedometer2):
  """Speedometer2 benchmark with the PCSscan flag.

  Shows the performance of upcoming PCScan feature.
  """

  @classmethod
  def Name(cls):
    return 'speedometer2-pcscan'

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-features=PartitionAllocPCScan')
