# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import decorators
from telemetry.page import page
from telemetry.testing import options_for_unittests
from telemetry.testing import page_test_test_case
from telemetry.util import wpr_modes

from measurements import thread_times
from metrics import timeline


class AnimatedPage(page.Page):

  def __init__(self, page_set):
    super(AnimatedPage, self).__init__(
        url='file://animated_page.html',
        page_set=page_set, base_dir=page_set.base_dir)

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(.2)


class ThreadTimesUnitTest(page_test_test_case.PageTestTestCase):

  def setUp(self):
    self._options = options_for_unittests.GetCopy()
    self._options.browser_options.wpr_mode = wpr_modes.WPR_OFF

  @decorators.Disabled('android')
  def testBasic(self):
    ps = self.CreateStorySetFromFileInUnittestDataDir('scrollable_page.html')
    measurement = thread_times.ThreadTimes()
    timeline_options = self._options
    results = self.RunMeasurement(measurement, ps, options=timeline_options)
    self.assertFalse(results.had_failures)

    for category in timeline.TimelineThreadCategories.values():
      cpu_time_name = timeline.ThreadCpuTimeResultName(category)
      cpu_time = results.FindAllPageSpecificValuesNamed(cpu_time_name)
      self.assertEquals(len(cpu_time), 1)

  @decorators.Disabled('chromeos')  # crbug.com/483212
  def testWithSilkDetails(self):
    ps = self.CreateStorySetFromFileInUnittestDataDir('scrollable_page.html')
    measurement = thread_times.ThreadTimes(report_silk_details=True)
    results = self.RunMeasurement(measurement, ps, options=self._options)
    self.assertFalse(results.had_failures)

    main_thread = 'renderer_main'
    expected_trace_categories = ['blink', 'cc', 'idle']
    for trace_category in expected_trace_categories:
      value_name = timeline.ThreadDetailResultName(
          main_thread, trace_category)
      values = results.FindAllPageSpecificValuesNamed(value_name)
      self.assertEquals(len(values), 1)

  def testCleanUpTrace(self):
    self.TestTracingCleanedUp(thread_times.ThreadTimes, self._options)
