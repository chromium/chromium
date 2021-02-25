# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from benchmarks import system_health
from telemetry import benchmark

from contrib.system_health_scroll_jank import janky_story_set


@benchmark.Info(emails=['khokhlov@google.com'])
class SystemHealthScrollJankMobile(system_health.MobileCommonSystemHealth):
  """A subset of system_health.common_mobile benchmark.

  Contains only stories related to monitoring jank during scrolling.
  This benchmark is used for running experimental scroll jank metrics.

  """

  @classmethod
  def Name(cls):
    return 'system_health.scroll_jank_mobile'

  def CreateCoreTimelineBasedMeasurementOptions(self):
    options = super(SystemHealthScrollJankMobile,
                    self).CreateCoreTimelineBasedMeasurementOptions()
    options.ExtendTraceCategoryFilter(['benchmark', 'cc', 'input'])
    options.SetTimelineBasedMetrics([
        'tbmv3:janky_time_per_scroll_processing_time',
        'tbmv3:num_excessive_touch_moves_blocking_gesture_scroll_updates',
    ])
    return options

  def CreateStorySet(self, options):
    return janky_story_set.JankyStorySet()
