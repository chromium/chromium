# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import time

from core import perf_benchmark
from core import platforms

from telemetry import benchmark
from telemetry import page as page_module
from telemetry import story
from telemetry.web_perf import timeline_based_measurement


class _Download4M(page_module.Page):
  def RunPageInteractions(self, action_runner):
    action_runner.ClickElement(text='4M')
    time.sleep(100)


class _Download10x400K(page_module.Page):
  def RunPageInteractions(self, action_runner):
    for _ in range(10):
      action_runner.ClickElement(text='400K')
      time.sleep(10)


@benchmark.Info(emails=['chrometto-team@google.com'])
class DownloadMobile(perf_benchmark.PerfBenchmark):
  """A benchmark to measure the power effect of downloading one big vs
  several small files.

  Note that it always uses the live storage.googleapis.com site, even if the
  --use-live-sites option is not set.
  """

  SUPPORTED_PLATFORM_TAGS = [platforms.ANDROID]

  def CreateStorySet(self, options):
    ps = story.StorySet()
    page_url = 'file://' + os.path.join(os.path.dirname(__file__), 'page.html')
    ps.AddStory(_Download4M(page_url, ps, name='download_4M'))
    ps.AddStory(_Download10x400K(page_url, ps, name='download_10x400K'))
    return ps

  def CreateCoreTimelineBasedMeasurementOptions(self):
    options = timeline_based_measurement.Options()
    options.config.enable_experimental_system_tracing = True
    options.config.system_trace_config.EnablePower()
    options.SetTimelineBasedMetrics(['tbmv3:power_rails_metric'])
    return options

  def SetExtraBrowserOptions(self, options):
    # Note that this overwrites the --proxy-bypass-list argument
    # set in chrome_startup_args.py.
    options.AppendExtraBrowserArgs([
        '--proxy-bypass-list=<-loopback>;storage.googleapis.com',
    ])

  @classmethod
  def Name(cls):
    return 'download.mobile'
