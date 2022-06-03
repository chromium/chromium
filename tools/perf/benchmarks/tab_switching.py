# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from core import perf_benchmark
from core import platforms

from page_sets.system_health import multi_tab_stories
from telemetry import benchmark
from telemetry import story
from telemetry.timeline import chrome_trace_category_filter
from telemetry.web_perf import timeline_based_measurement


@benchmark.Info(emails=['vovoy@chromium.org'],
                 component='OS>Performance')
class TabSwitchingTypical25(perf_benchmark.PerfBenchmark):
  """This test records the MPArch.RWH_TabSwitchPaintDuration histogram.

  The histogram is a measure of the time between when a tab was requested to be
  shown, and when first paint occurred. The script opens 25 pages in different
  tabs, waits for them to load, and then switches to each tab and records the
  metric. The pages were chosen from Alexa top ranking sites.
  """
  # TODO(rmhasan): Remove the SUPPORTED_PLATFORMS lists.
  # SUPPORTED_PLATFORMS is deprecated, please put system specifier tags
  # from expectations.config in SUPPORTED_PLATFORM_TAGS.
  SUPPORTED_PLATFORMS = [story.expectations.ALL_DESKTOP]
  SUPPORTED_PLATFORM_TAGS = [platforms.DESKTOP]

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    parser.add_option('--tabset-repeat', type='int', default=1,
                      help='repeat tab page set')

  def CreateStorySet(self, options):
    story_set = story.StorySet(
        archive_data_file='../page_sets/data/system_health_desktop.json',
        base_dir=os.path.dirname(os.path.abspath(__file__)),
        cloud_storage_bucket=story.PARTNER_BUCKET)
    story_set.AddStory(multi_tab_stories.MultiTabTypical24Story(
        story_set, False, options.tabset_repeat))
    return story_set

  def CreateCoreTimelineBasedMeasurementOptions(self):
    category_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter()
    category_filter.AddIncludedCategory('latency')
    options = timeline_based_measurement.Options(category_filter)
    options.SetTimelineBasedMetrics(['tabsMetric'])
    return options

  @classmethod
  def Name(cls):
    return 'tab_switching.typical_25'
