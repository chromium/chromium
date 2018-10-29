# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from telemetry.testing import test_page_test_results
from telemetry.timeline import model as model_module
from telemetry.web_perf import timeline_interaction_record as tir_module

from metrics import timeline


def _GetInteractionRecord(start, end):
  return tir_module.TimelineInteractionRecord('test-record', start, end)


class ThreadTimesTimelineMetricUnittest(unittest.TestCase):

  def GetResults(self, metric, model, renderer_thread, interaction_record):
    results = test_page_test_results.TestPageTestResults(self)
    metric.AddResults(model, renderer_thread, interaction_record,
                      results)
    return results

  def testResults(self):
    model = model_module.TimelineModel()
    renderer_main = model.GetOrCreateProcess(1).GetOrCreateThread(2)
    renderer_main.name = 'CrRendererMain'

    metric = timeline.ThreadTimesTimelineMetric()
    metric.details_to_report = timeline.ReportMainThreadOnly
    results = self.GetResults(metric, model, renderer_main.parent,
                              [_GetInteractionRecord(1, 2)])

    # Test that all result thread categories exist
    for name in timeline.TimelineThreadCategories.values():
      results.GetPageSpecificValueNamed(
          timeline.ThreadCpuTimeResultName(name))

  def testBasic(self):
    model = model_module.TimelineModel()
    renderer_main = model.GetOrCreateProcess(1).GetOrCreateThread(2)
    renderer_main.name = 'CrRendererMain'

    # Create two frame swaps (Results times should be divided by two) for
    # an interaction that lasts 20 milliseconds.
    cc_main = model.GetOrCreateProcess(1).GetOrCreateThread(3)
    cc_main.name = 'Compositor'
    cc_main.BeginSlice('cc_cat', timeline.FrameTraceName, 10, 10,
        args={'step': 'GenerateCompositorFrame'})
    cc_main.EndSlice(11, 11)
    cc_main.BeginSlice('cc_cat', timeline.FrameTraceName, 12, 12,
        args={'step': 'GenerateCompositorFrame'})
    cc_main.EndSlice(13, 13)

    # [      X       ]   [ Z ]
    #      [  Y  ]
    renderer_main.BeginSlice('cat1', 'X', 10, 0)
    renderer_main.BeginSlice('cat2', 'Y', 15, 5)
    renderer_main.EndSlice(16, 5.5)
    renderer_main.EndSlice(30, 19.5)
    renderer_main.BeginSlice('cat1', 'Z', 31, 20)
    renderer_main.BeginSlice('cat1', 'Z', 33, 21)
    model.FinalizeImport()

    # Exclude 'Z' using an action-range.
    metric = timeline.ThreadTimesTimelineMetric()
    metric.details_to_report = timeline.ReportMainThreadOnly
    results = self.GetResults(metric, model, renderer_main.parent,
                              [_GetInteractionRecord(10, 30)])

    # Test for the results we expect.
    main_thread = 'renderer_main'
    cc_thread = 'renderer_compositor'
    assert_results = [
        (timeline.ThreadMeanFrameTimeResultName(cc_thread), 'ms', 10.0),
        (timeline.ThreadTasksResultName(main_thread), 'tasks', 0.5),
        (timeline.ThreadTasksResultName(cc_thread), 'tasks', 1.0),
        (timeline.ThreadCpuTimeResultName(main_thread), 'ms', 9.75),
        (timeline.ThreadCpuTimeResultName(cc_thread), 'ms', 1.0),
        (timeline.ThreadDetailResultName(main_thread, 'cat1'), 'ms', 9.5),
        (timeline.ThreadDetailResultName(main_thread, 'cat2'), 'ms', 0.5),
        (timeline.ThreadDetailResultName(main_thread, 'idle'), 'ms', 0),
    ]
    for name, unit, value in assert_results:
      results.AssertHasPageSpecificScalarValue(name, unit, value)

  def testOverheadIsRemoved(self):
    model = model_module.TimelineModel()
    renderer_main = model.GetOrCreateProcess(1).GetOrCreateThread(2)
    renderer_main.name = 'CrRendererMain'

    # Create one frame swap.
    cc_main = model.GetOrCreateProcess(1).GetOrCreateThread(3)
    cc_main.name = 'Compositor'
    cc_main.BeginSlice('cc_cat', timeline.FrameTraceName, 10, 10,
        args={'step': 'GenerateCompositorFrame'})
    cc_main.EndSlice(11, 11)

    # [      X       ]
    #    [Overhead]
    overhead_category = timeline.OverheadTraceCategory
    overhead_name = timeline.OverheadTraceName
    renderer_main.BeginSlice('cat1', 'X', 10, 0)
    renderer_main.BeginSlice(overhead_category, overhead_name, 15, 5)
    renderer_main.EndSlice(16, 6)
    renderer_main.EndSlice(30, 10)
    model.FinalizeImport()

    # Include everything in an action-range.
    metric = timeline.ThreadTimesTimelineMetric()
    metric.details_to_report = timeline.ReportMainThreadOnly
    results = self.GetResults(metric, model, renderer_main.parent,
                              [_GetInteractionRecord(10, 30)])

    # Test a couple specific results.
    assert_results = [
        (timeline.ThreadCpuTimeResultName('renderer_main'), 'ms', 9.0),
    ]
    for name, unit, value in assert_results:
      results.AssertHasPageSpecificScalarValue(name, unit, value)
