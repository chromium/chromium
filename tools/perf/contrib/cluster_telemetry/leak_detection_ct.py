# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import py_utils
from contrib.leak_detection import leak_detection as ld

from contrib.cluster_telemetry import ct_benchmarks_util
from contrib.cluster_telemetry import page_set

# pylint: disable=protected-access
class LeakDetectionClusterTelemetry(ld._LeakDetectionBase):

  options = {'upload_results': True}

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    super(LeakDetectionClusterTelemetry,
          cls).AddBenchmarkCommandLineArgs(parser)
    ct_benchmarks_util.AddBenchmarkCommandLineArgs(parser)

  def CreateStorySet(self, options):
    def RunNavigateSteps(self, action_runner):
        action_runner.Navigate('about:blank')
        action_runner.PrepareForLeakDetection()
        action_runner.MeasureMemory(True)
        action_runner.Navigate(self.url)
        try:
          py_utils.WaitFor(action_runner.tab.HasReachedQuiescence, timeout=30)
        except py_utils.TimeoutException:
          # Conduct leak detection whether or not loading has finished
          pass
        action_runner.Navigate('about:blank')
        action_runner.PrepareForLeakDetection()
        action_runner.MeasureMemory(True)
    return page_set.CTPageSet(
      options.urls_list, options.user_agent, options.archive_data_file,
      run_navigate_steps_callback=RunNavigateSteps)

  @classmethod
  def Name(cls):
    return 'leak_detection.cluster_telemetry'
