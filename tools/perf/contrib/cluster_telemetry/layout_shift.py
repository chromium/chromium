# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from contrib.cluster_telemetry import loading_base_ct
from contrib.cluster_telemetry import page_set

# pylint: disable=protected-access


class LayoutShiftClusterTelemetry(loading_base_ct._LoadingBaseClusterTelemetry):
  @classmethod
  def Name(cls):
    return 'layout_shift.cluster_telemetry'

  def CreateCoreTimelineBasedMeasurementOptions(self):
    options = super(LayoutShiftClusterTelemetry,
                    self).CreateCoreTimelineBasedMeasurementOptions()
    options.AddTraceCategoryFilter('benchmark')

    options.SetTimelineBasedMetrics(['loadingMetric', 'countSumMetric'])
    return options

  def CreateStorySet(self, options):
    def Wait(action_runner):
      action_runner.Wait(options.wait_time)

    def RunNavigateSteps(self, action_runner):
      action_runner.StartMobileDeviceEmulation(360, 640, 2)
      action_runner.Navigate(self.url)

    return page_set.CTPageSet(options.urls_list,
                              options.user_agent,
                              options.archive_data_file,
                              traffic_setting=options.traffic_setting,
                              cache_temperature=options.cache_temperature,
                              run_page_interaction_callback=Wait,
                              run_navigate_steps_callback=RunNavigateSteps)
