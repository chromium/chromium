# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""GP performance benchmark.
"""

from benchmarks import press

from telemetry import story
from telemetry import benchmark
from telemetry.timeline import chrome_trace_category_filter
from telemetry.web_perf import timeline_based_measurement

import page_sets

DOC_URL_ = ("https://docs.google.com/document/d/"
            "1azH3HnV0IIbd1NtcX_8K_E_s_aJVS45MqkRCg-DT8tE/edit?"
            "usp=sharing&resourcekey=0-mW7mSVdRfxbQezL6ZdF3Fg")


class _GP(press._PressBenchmark):  # pylint: disable=protected-access
  """GP Benchmark.
  """

  enable_smoke_test_mode = False
  enable_systrace = False
  extra_chrome_categories = False
  enable_rcs = False
  iteration_count = None
  show_iteration_metrics = False

  def CreateStorySet(self, options):
    # For a smoke test one iteration is sufficient
    if self.enable_smoke_test_mode and not self.iteration_count:
      iteration_count = 1
    else:
      iteration_count = self.iteration_count

    if self.PLATFORM == 'mobile':
      page_set_cls = page_sets.GPMobileStorySet2022
    elif self.PLATFORM == 'desktop':
      page_set_cls = page_sets.GPDesktopStorySet2022
    else:
      raise Exception("Unknown platform: %s" % self.PLATFORM)
    return page_set_cls(iteration_count=iteration_count,
                        show_iteration_metrics=self.show_iteration_metrics)

  def CreateCoreTimelineBasedMeasurementOptions(self):
    if not self.enable_systrace:
      return timeline_based_measurement.Options()

    cat_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter()

    # "blink.console" is used for marking ranges in
    # cache_temperature.MarkTelemetryInternal.
    cat_filter.AddIncludedCategory('blink.console')

    # "toplevel" category is used to capture TaskQueueManager events.
    cat_filter.AddIncludedCategory('toplevel')

    if self.extra_chrome_categories:
      cat_filter.AddFilterString(self.extra_chrome_categories)

    if self.enable_rcs:
      # V8 needed categories
      cat_filter.AddIncludedCategory('v8')
      cat_filter.AddDisabledByDefault('disabled-by-default-v8.runtime_stats')

      tbm_options = timeline_based_measurement.Options(
          overhead_level=cat_filter)
      tbm_options.SetTimelineBasedMetrics(['runtimeStatsTotalMetric'])
      return tbm_options

    tbm_options = timeline_based_measurement.Options(overhead_level=cat_filter)
    tbm_options.SetTimelineBasedMetrics(['tracingMetric'])
    return tbm_options

  def SetExtraBrowserOptions(self, options):
    if self.enable_rcs:
      options.AppendExtraBrowserArgs(
          '--enable-blink-features=BlinkRuntimeCallStats')

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    parser.add_option('--enable-rcs',
                      action="store_true",
                      help="Enables runtime call stats")
    parser.add_option('--show-iteration-metrics',
                      action="store_true",
                      help="Show the total metrics for each iteration")
    parser.add_option('--iteration-count',
                      type="int",
                      help="Override the default number of iterations")

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    if args.enable_systrace or args.enable_rcs:
      cls.enable_systrace = True
    if args.extra_chrome_categories:
      cls.extra_chrome_categories = args.extra_chrome_categories
    if args.enable_rcs:
      cls.enable_rcs = True
    if args.show_iteration_metrics:
      cls.show_iteration_metrics = False
    if args.iteration_count:
      cls.iteration_count = args.iteration_count


@benchmark.Info(emails=['vahl@chromium.org', 'cbruni@chromium.org'],
                component='Blink>JavaScript',
                documentation_url=DOC_URL_)
class GP2022Desktop(_GP):
  PLATFORM = 'desktop'
  SUPPORTED_PLATFORMS = [story.expectations.ALL_DESKTOP]

  @classmethod
  def Name(cls):
    return 'UNSCHEDULED_GP-desktop'


@benchmark.Info(emails=['vahl@chromium.org', 'cbruni@chromium.org'],
                component='Blink>JavaScript',
                documentation_url=DOC_URL_)
class GP2022Mobile(_GP):
  PLATFORM = 'mobile'
  SUPPORTED_PLATFORMS = [story.expectations.ALL_MOBILE]

  @classmethod
  def Name(cls):
    return 'UNSCHEDULED_GP-mobile'


class _V8FutureMixin(object):
  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-features=V8VmFuture')


@benchmark.Info(emails=['vahl@chromium.org', 'cbruni@chromium.org'],
                component='Blink>JavaScript',
                documentation_url=DOC_URL_)
class V8GP2022FutureDesktop(GP2022Desktop, _V8FutureMixin):
  """GP benchmark with the V8 flag --future.
  Shows the performance of upcoming V8 VM features.
  """

  @classmethod
  def Name(cls):
    return 'UNSCHEDULED_GP-desktop-future'


@benchmark.Info(emails=['vahl@chromium.org', 'cbruni@chromium.org'],
                component='Blink>JavaScript',
                documentation_url=DOC_URL_)
class V8GP2022FutureMobile(GP2022Mobile, _V8FutureMixin):
  """GP benchmark with the V8 flag --future.
  Shows the performance of upcoming V8 VM features.
  """

  @classmethod
  def Name(cls):
    return 'UNSCHEDULED_GP-mobile-future'
