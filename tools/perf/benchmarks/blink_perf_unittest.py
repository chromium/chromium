# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import unittest

from telemetry import story
from telemetry.testing import options_for_unittests
from telemetry.testing import page_test_test_case
from telemetry.timeline import async_slice
from telemetry.timeline import model as model_module


from benchmarks import blink_perf


class BlinkPerfTest(page_test_test_case.PageTestTestCase):
  _BLINK_PERF_TEST_DATA_DIR = os.path.join(os.path.dirname(__file__),
      '..', '..', '..', 'third_party', 'blink', 'perf_tests',
      'test_data')

  _BLINK_PERF_RESOURCES_DIR = os.path.join(os.path.dirname(__file__),
      '..', '..', '..', 'third_party', 'blink', 'perf_tests',
      'resources')
  def setUp(self):
    self._options = options_for_unittests.GetCopy()
    # pylint: disable=protected-access
    self._measurement = blink_perf._BlinkPerfMeasurement()
    # pylint: enable=protected-access

  def _CreateStorySetForTestFile(self, test_file_name):
    story_set = story.StorySet(base_dir=self._BLINK_PERF_TEST_DATA_DIR,
        serving_dirs={self._BLINK_PERF_TEST_DATA_DIR,
                      self._BLINK_PERF_RESOURCES_DIR})
    # pylint: disable=protected-access
    page = blink_perf._BlinkPerfPage('file://' + test_file_name, story_set,
        base_dir=story_set.base_dir, name=test_file_name)
    # pylint: enable=protected-access
    story_set.AddStory(page)
    return story_set

  def testBlinkPerfTracingMetricsForMeasureTime(self):
    results = self.RunMeasurement(measurement=self._measurement,
        ps=self._CreateStorySetForTestFile('append-child-measure-time.html'),
        options=self._options)
    self.assertFalse(results.had_failures)
    self.assertEquals(len(results.FindAllTraceValues()), 1)

    frame_view_layouts = results.FindAllPageSpecificValuesNamed(
        'LocalFrameView::layout')
    self.assertEquals(len(frame_view_layouts), 1)
    # append-child-measure-time.html specifies 5 iterationCount.
    self.assertEquals(len(frame_view_layouts[0].values), 5)
    self.assertGreater(frame_view_layouts[0].GetRepresentativeNumber(), 0.001)

    update_layout_trees = results.FindAllPageSpecificValuesNamed(
        'UpdateLayoutTree')
    self.assertEquals(len(update_layout_trees), 1)
    # append-child-measure-time.html specifies 5 iterationCount.
    self.assertEquals(len(update_layout_trees[0].values), 5)

    self.assertGreater(update_layout_trees[0].GetRepresentativeNumber(), 0.001)

  def testBlinkPerfTracingMetricsForMeasureFrameTime(self):
    results = self.RunMeasurement(measurement=self._measurement,
        ps=self._CreateStorySetForTestFile(
            'color-changes-measure-frame-time.html'),
        options=self._options)
    self.assertFalse(results.had_failures)
    self.assertEquals(len(results.FindAllTraceValues()), 1)

    frame_view_prepaints = results.FindAllPageSpecificValuesNamed(
        'LocalFrameView::RunPrePaintLifecyclePhase')

    self.assertEquals(len(frame_view_prepaints), 1)
    # color-changes-measure-frame-time.html specifies 9 iterationCount.
    self.assertEquals(len(frame_view_prepaints[0].values), 9)
    self.assertGreater(frame_view_prepaints[0].GetRepresentativeNumber(), 0.001)

    frame_view_painttrees = results.FindAllPageSpecificValuesNamed(
        'LocalFrameView::RunPaintLifecyclePhase')
    self.assertEquals(len(frame_view_painttrees), 1)
    # color-changes-measure-frame-time.html specifies 9 iterationCount.
    self.assertEquals(len(frame_view_painttrees[0].values), 9)
    self.assertGreater(frame_view_painttrees[0].GetRepresentativeNumber(),
        0.001)

  def testBlinkPerfTracingMetricsForMeasurePageLoadTime(self):
    results = self.RunMeasurement(measurement=self._measurement,
        ps=self._CreateStorySetForTestFile(
            'simple-html-measure-page-load-time.html'),
        options=self._options)
    self.assertFalse(results.had_failures)
    self.assertEquals(len(results.FindAllTraceValues()), 1)

    create_child_frame = results.FindAllPageSpecificValuesNamed(
        'WebLocalFrameImpl::createChildframe')
    self.assertEquals(len(create_child_frame), 1)
    # color-changes-measure-frame-time.html specifies 7 iterationCount.
    self.assertEquals(len(create_child_frame[0].values), 7)
    self.assertGreater(create_child_frame[0].GetRepresentativeNumber(), 0.001)

    post_layout_task = results.FindAllPageSpecificValuesNamed(
        'LocalFrameView::performPostLayoutTasks')
    self.assertEquals(len(post_layout_task), 1)
    # color-changes-measure-frame-time.html specifies 7 iterationCount.
    self.assertEquals(len(post_layout_task[0].values), 7)
    self.assertGreater(post_layout_task[0].GetRepresentativeNumber(), 0.001)


  def testBlinkPerfTracingMetricsForMeasureAsync(self):
    results = self.RunMeasurement(measurement=self._measurement,
        ps=self._CreateStorySetForTestFile(
            'simple-blob-measure-async.html'),
        options=self._options)
    self.assertFalse(results.failures)
    self.assertEquals(len(results.FindAllTraceValues()), 1)

    blob_requests = results.FindAllPageSpecificValuesNamed(
        'BlobRequest')
    blob_readers = results.FindAllPageSpecificValuesNamed(
        'BlobReader')
    self.assertEquals(len(blob_requests), 1)
    self.assertEquals(len(blob_readers), 1)
    # simple-blob-measure-async.html specifies 6 iterationCount.
    self.assertEquals(len(blob_requests[0].values), 6)
    self.assertEquals(len(blob_readers[0].values), 6)

    # TODO(mek): Delete non-mojo code paths when blobs are always using mojo.
    using_mojo = blob_readers[0].GetRepresentativeNumber() > 0.001
    if using_mojo:
      self.assertEquals(blob_requests[0].GetRepresentativeNumber(), 0)
      self.assertGreater(blob_readers[0].GetRepresentativeNumber(), 0.001)
    else:
      self.assertGreater(blob_requests[0].GetRepresentativeNumber(), 0.001)
      self.assertEquals(blob_readers[0].GetRepresentativeNumber(), 0)

    if using_mojo:
      read_data = results.FindAllPageSpecificValuesNamed(
          'BlobReader::ReadMore')
    else:
      read_data = results.FindAllPageSpecificValuesNamed(
          'BlobRequest::ReadRawData')
    self.assertEquals(len(read_data), 1)
    # simple-blob-measure-async.html specifies 6 iterationCount.
    self.assertEquals(len(read_data[0].values), 6)
    self.assertGreater(read_data[0].GetRepresentativeNumber(), 0.001)


# pylint: disable=protected-access
# This is needed for testing _ComputeTraceEventsThreadTimeForBlinkPerf method.
class ComputeTraceEventsMetricsForBlinkPerfTest(unittest.TestCase):

  def _AddAsyncSlice(self, renderer_thread, category, name, start, end):
    s = async_slice.AsyncSlice(
        category, name,
        timestamp=start, duration=end - start, start_thread=renderer_thread,
        end_thread=renderer_thread)
    renderer_thread.AddAsyncSlice(s)

  def _AddBlinkTestSlice(self, renderer_thread, start, end):
    self._AddAsyncSlice(
        renderer_thread, 'blink', 'blink_perf.runTest', start, end)

  def testTraceEventMetricsSingleBlinkTest(self):
    model = model_module.TimelineModel()
    renderer_main = model.GetOrCreateProcess(1).GetOrCreateThread(2)
    renderer_main.name = 'CrRendererMain'

    # Set up a main thread model that looks like:
    #   [          blink_perf.run_test                ]
    #   |     [ foo ]          [  bar ]      [        baz     ]
    #   |     |     |          |      |      |        |       |
    #   100   120   140        400    420    500      550     600
    #             |                |                  |
    # CPU dur:    15               18                 70
    #
    self._AddBlinkTestSlice(renderer_main, 100, 550)

    renderer_main.BeginSlice('blink', 'foo', 120, 122)
    renderer_main.EndSlice(140, 137)

    renderer_main.BeginSlice('blink', 'bar', 400, 402)
    renderer_main.EndSlice(420, 420)

    # Since this "baz" slice has CPU duration = 70ms, wall-time duration = 100ms
    # & its overalapped wall-time with "blink_perf.run_test" is 50 ms, its
    # overlapped CPU time with "blink_perf.run_test" is
    # 50 * 70 / 100 = 35ms.
    renderer_main.BeginSlice('blink', 'baz', 500, 520)
    renderer_main.EndSlice(600, 590)

    self.assertEquals(
        blink_perf._ComputeTraceEventsThreadTimeForBlinkPerf(
            model, renderer_main, ['foo', 'bar', 'baz']),
        {'foo': [15], 'bar': [18], 'baz': [35]})

  def testTraceEventMetricsMultiBlinkTest(self):
    model = model_module.TimelineModel()
    renderer_main = model.GetOrCreateProcess(1).GetOrCreateThread(2)
    renderer_main.name = 'CrRendererMain'

    # Set up a main thread model that looks like:
    #   [          blink_perf.run_test    ]         [ blink_perf.run_test  ]
    #   |     [ foo ]          [  bar ]   |     [   |    foo         ]     |
    #   |     |     |          |      |   |     |   |     |          |     |
    #   100   120   140        400    420 440   500 520              600   640
    #             |                |                      |
    # CPU dur:    15               18                     40
    #
    self._AddBlinkTestSlice(renderer_main, 100, 440)
    self._AddBlinkTestSlice(renderer_main, 520, 640)

    renderer_main.BeginSlice('blink', 'foo', 120, 122)
    renderer_main.EndSlice(140, 137)

    renderer_main.BeginSlice('blink', 'bar', 400, 402)
    renderer_main.EndSlice(420, 420)

    # Since this "foo" slice has CPU duration = 40ms, wall-time duration = 100ms
    # & its overalapped wall-time with "blink_perf.run_test" is 80 ms, its
    # overlapped CPU time with "blink_perf.run_test" is
    # 80 * 40 / 100 = 32ms.
    renderer_main.BeginSlice('blink', 'foo', 500, 520)
    renderer_main.EndSlice(600, 560)

    self.assertEquals(
        blink_perf._ComputeTraceEventsThreadTimeForBlinkPerf(
            model, renderer_main, ['foo', 'bar', 'baz']),
        {'foo': [15, 32], 'bar': [18, 0], 'baz': [0, 0]})

  def testTraceEventMetricsNoThreadTimeAvailable(self):
    model = model_module.TimelineModel()
    renderer_main = model.GetOrCreateProcess(1).GetOrCreateThread(2)
    renderer_main.name = 'CrRendererMain'

    # Set up a main thread model that looks like:
    #   [          blink_perf.run_test                ]
    #   |     [ foo ]          [  bar ]               |
    #   |     |     |          |      |               |
    #   100   120   140        400    420             550
    #             |                |
    # CPU dur:    None             None
    #
    self._AddBlinkTestSlice(renderer_main, 100, 550)

    renderer_main.BeginSlice('blink', 'foo', 120)
    renderer_main.EndSlice(140)

    renderer_main.BeginSlice('blink', 'bar', 400)
    renderer_main.EndSlice(420)

    self.assertEquals(
        blink_perf._ComputeTraceEventsThreadTimeForBlinkPerf(
            model, renderer_main, ['foo', 'bar']),
        {'foo': [20], 'bar': [20]})

  def testTraceEventMetricsMultiBlinkTestCrossProcesses(self):
    model = model_module.TimelineModel()
    renderer_main = model.GetOrCreateProcess(1).GetOrCreateThread(2)
    renderer_main.name = 'CrRendererMain'

    foo_thread = model.GetOrCreateProcess(2).GetOrCreateThread(4)
    bar_thread =  model.GetOrCreateProcess(2).GetOrCreateThread(5)

    # Set up a main model that looks like (P1 & P2 are different processes):
    # P1  [          blink_perf.run_test    ]         [ blink_perf.run_test  ]
    #     |                                 |         |                      |
    # P2  |     [ foo ]                     |     [   |    foo         ]     |
    #     |     |  |  |          [  bar ]   |     |   |     |          |
    #     |     |  |  |          |   |  |   |     |   |     |          |     |
    #    100   120 | 140        400  | 420 440   500 520    |         600   640
    #              |                 |                      |
    # CPU dur:    15                N/A                     40
    #
    self._AddBlinkTestSlice(renderer_main, 100, 440)
    self._AddBlinkTestSlice(renderer_main, 520, 640)

    foo_thread.BeginSlice('blink', 'foo', 120, 122)
    foo_thread.EndSlice(140, 137)

    bar_thread.BeginSlice('blink', 'bar', 400)
    bar_thread.EndSlice(420)

    # Since this "foo" slice has CPU duration = 40ms, wall-time duration = 100ms
    # & its overalapped wall-time with "blink_perf.run_test" is 80 ms, its
    # overlapped CPU time with "blink_perf.run_test" is
    # 80 * 40 / 100 = 32ms.
    foo_thread.BeginSlice('blink', 'foo', 500, 520)
    foo_thread.EndSlice(600, 560)

    self.assertEquals(
        blink_perf._ComputeTraceEventsThreadTimeForBlinkPerf(
            model, renderer_main, ['foo', 'bar', 'baz']),
        {'foo': [15, 32], 'bar': [20, 0], 'baz': [0, 0]})

  def testTraceEventMetricsNoDoubleCountingBasic(self):
    model = model_module.TimelineModel()
    renderer_main = model.GetOrCreateProcess(1).GetOrCreateThread(2)
    renderer_main.name = 'CrRendererMain'

    # Set up a main thread model that looks like:
    #   [          blink_perf.run_test                     ]
    #   |     [          foo           ]     [  foo  ]     |
    #   |     [          foo           ]     |       |     |
    #   |     |     [    foo     ]     |     |       |     |
    #   |     |     |            |     |     |       |     |
    #   100   120  140          400   420   440     510   550
    #                     |                      |
    # CPU dur of          |                      |
    # of top most event:  280                    50
    #
    self._AddBlinkTestSlice(renderer_main, 100, 550)

    renderer_main.BeginSlice('blink', 'foo', 120, 130)
    renderer_main.BeginSlice('blink', 'foo', 120, 130)
    renderer_main.BeginSlice('blink', 'foo', 140, 150)
    renderer_main.EndSlice(400, 390)
    renderer_main.EndSlice(420, 410)
    renderer_main.EndSlice(420, 410)

    renderer_main.BeginSlice('blink', 'foo', 440, 455)
    renderer_main.EndSlice(510, 505)

    self.assertEquals(
        blink_perf._ComputeTraceEventsThreadTimeForBlinkPerf(
            model, renderer_main, ['foo']), {'foo': [330]})


  def testTraceEventMetricsNoDoubleCountingWithOtherSlidesMixedIn(self):
    model = model_module.TimelineModel()
    renderer_main = model.GetOrCreateProcess(1).GetOrCreateThread(2)
    renderer_main.name = 'CrRendererMain'

    # Set up a main thread model that looks like:
    #   [          blink_perf.run_test                                   ]
    #   |     [          foo                 ]      [      bar     ]     |
    #   |     |   [        bar         ]     |      |    [ foo  ]  |     |
    #   |     |   |    [    foo     ]  |     |      |    |      |  |     |
    #   |     |   |    |            |  |     |      |    |      |  |     |
    #   100   120 130 140     |    400 405   420    440  480 | 510 520  550
    #                         |                              |
    # CPU dur of              |                              |
    # of top most event:  280 (foo) & 270 (bar)            50 (bar) & 20 (foo)
    #
    self._AddBlinkTestSlice(renderer_main, 100, 550)

    renderer_main.BeginSlice('blink', 'foo', 120, 130)
    renderer_main.BeginSlice('blink', 'bar', 130, 135)
    renderer_main.BeginSlice('blink', 'foo', 140, 150)
    renderer_main.EndSlice(400, 390)
    renderer_main.EndSlice(405, 405)
    renderer_main.EndSlice(420, 410)

    renderer_main.BeginSlice('blink', 'bar', 440, 455)
    renderer_main.BeginSlice('blink', 'foo', 480, 490)
    renderer_main.EndSlice(510, 510)
    renderer_main.EndSlice(510, 505)

    self.assertEquals(
        blink_perf._ComputeTraceEventsThreadTimeForBlinkPerf(
            model, renderer_main, ['foo', 'bar']),
            {'foo': [300], 'bar': [320]})

  def testAsyncTraceEventMetricsOverlapping(self):
    model = model_module.TimelineModel()
    renderer_main = model.GetOrCreateProcess(1).GetOrCreateThread(2)
    renderer_main.name = 'CrRendererMain'

    # Set up a main thread model that looks like:
    #   [           blink_perf.run_test                ]
    #   |       [  foo  ]        [ bar ]               |
    #   |   [  foo  ]   |        |     |               |
    #   |   |   |   |   |        |     |               |
    #   100 110 120 130 140      400   420             550
    # CPU dur: None for all.
    #
    self._AddBlinkTestSlice(renderer_main, 100, 550)

    self._AddAsyncSlice(renderer_main, 'blink', 'foo', 110, 130)
    self._AddAsyncSlice(renderer_main, 'blink', 'foo', 120, 140)
    self._AddAsyncSlice(renderer_main, 'blink', 'bar', 400, 420)

    self.assertEquals(
        blink_perf._ComputeTraceEventsThreadTimeForBlinkPerf(
            model, renderer_main, ['foo', 'bar']),
        {'foo': [30], 'bar': [20]})

  def testAsyncTraceEventMetricsMultipleTests(self):
    model = model_module.TimelineModel()
    renderer_main = model.GetOrCreateProcess(1).GetOrCreateThread(2)
    renderer_main.name = 'CrRendererMain'

    # Set up a main model that looks like:
    #        [ blink_perf.run_test ]   [ blink_perf.run_test ]
    #        |                     |   |                     |
    #  [                        foo                              ]
    #  |  [                        foo                               ]
    #  |  |  |                     |   |                     |   |   |
    #  80 90 100                   200 300                   400 500 510
    # CPU dur: None for all
    #
    self._AddBlinkTestSlice(renderer_main, 100, 200)
    self._AddBlinkTestSlice(renderer_main, 300, 400)

    # Both events totally intersect both tests.
    self._AddAsyncSlice(renderer_main, 'blink', 'foo', 80, 500)
    self._AddAsyncSlice(renderer_main, 'blink', 'bar', 90, 510)

    self.assertEquals(
        blink_perf._ComputeTraceEventsThreadTimeForBlinkPerf(
            model, renderer_main, ['foo', 'bar']),
        {'foo': [100, 100], 'bar': [100, 100]})
