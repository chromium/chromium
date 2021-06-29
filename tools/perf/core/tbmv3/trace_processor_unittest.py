# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import tempfile
import unittest

from core.tbmv3 import trace_processor

import mock

RUN_METHOD = 'core.tbmv3.trace_processor._RunTraceProcessor'

class TraceProcessorTestCase(unittest.TestCase):
  def setUp(self):
    self.temp_dir = tempfile.mkdtemp()
    self.tp_path = os.path.join(self.temp_dir, 'trace_processor_shell')
    with open(self.tp_path, 'w'):
      pass
    with open(os.path.join(self.temp_dir, 'dummy_metric.sql'), 'w'):
      pass
    with open(os.path.join(self.temp_dir, 'dummy_metric.proto'), 'w'):
      pass

  def tearDown(self):
    shutil.rmtree(self.temp_dir)

  def testConvertProtoTraceToJson(self):
    with mock.patch(RUN_METHOD):
      trace_processor.ConvertProtoTraceToJson(
          self.tp_path, '/path/to/proto', '/path/to/json')

  def testRunMetricNoRepeated(self):
    metric_output = """
    {
      "perfetto.protos.dummy_metric": {
        "foo": 7,
        "bar": 8
       },
      "__annotations": {
        "perfetto.protos.dummy_metric": {
          "foo": {
            "__field_options": {
              "unit": "count_biggerIsBetter"
            }
          }
        }
      }
    }"""

    with mock.patch('core.tbmv3.trace_processor.METRICS_PATH', self.temp_dir):
      with mock.patch(RUN_METHOD) as run_patch:
        run_patch.return_value = metric_output
        histograms = trace_processor.RunMetric(
            self.tp_path, '/path/to/proto', 'dummy_metric')

    foo_hist = histograms.GetHistogramNamed('dummy::foo')
    self.assertEqual(foo_hist.unit, 'count_biggerIsBetter')
    self.assertEqual(foo_hist.sample_values, [7])

    bar_hists = histograms.GetHistogramsNamed('dummy::bar')
    self.assertEqual(len(bar_hists), 0)


  def testRunMetricRepeated(self):
    metric_output = """
    {
      "perfetto.protos.dummy_metric": {
        "foo": [4, 5, 6],
        "bar": [{"baz": 10}, {"baz": 11}]
       },
      "__annotations": {
        "perfetto.protos.dummy_metric": {
          "foo": {
            "__repeated": true,
            "__field_options": {
              "unit": "count_biggerIsBetter"
            }
          },
          "bar": {
            "__repeated": true,
            "baz": {
              "__field_options": {
                "unit": "ms_smallerIsBetter"
              }
            }
          }
        }
      }
    }"""

    with mock.patch('core.tbmv3.trace_processor.METRICS_PATH', self.temp_dir):
      with mock.patch(RUN_METHOD) as run_patch:
        run_patch.return_value = metric_output
        histograms = trace_processor.RunMetric(
            self.tp_path, '/path/to/proto', 'dummy_metric')

    foo_hist = histograms.GetHistogramNamed('dummy::foo')
    self.assertEqual(foo_hist.unit, 'count_biggerIsBetter')
    self.assertEqual(foo_hist.sample_values, [4, 5, 6])

    baz_hist = histograms.GetHistogramNamed('dummy::bar:baz')
    self.assertEqual(baz_hist.unit, 'ms_smallerIsBetter')
    self.assertEqual(baz_hist.sample_values, [10, 11])

  def testMarkedRepeatedButValueIsNotList(self):
    metric_output = """
    {
      "perfetto.protos.dummy_metric": {
        "foo": 4
       },
      "__annotations": {
        "perfetto.protos.dummy_metric": {
          "foo": {
            "__repeated": true,
            "__field_options": {
              "unit": "count_biggerIsBetter"
            }
          }
        }
      }
    }"""

    with mock.patch('core.tbmv3.trace_processor.METRICS_PATH', self.temp_dir):
      with mock.patch(RUN_METHOD) as run_patch:
        run_patch.return_value = metric_output
        with self.assertRaises(trace_processor.InvalidTraceProcessorOutput):
          trace_processor.RunMetric(
              self.tp_path, '/path/to/proto', 'dummy_metric')

  def testMarkedNotRepeatedButValueIsList(self):
    metric_output = """
    {
      "perfetto.protos.dummy_metric": {
        "foo": [1, 2, 3]
       },
      "__annotations": {
        "perfetto.protos.dummy_metric": {
          "foo": {
            "__field_options": {
              "unit": "count_biggerIsBetter"
            }
          }
        }
      }
    }"""

    with mock.patch('core.tbmv3.trace_processor.METRICS_PATH', self.temp_dir):
      with mock.patch(RUN_METHOD) as run_patch:
        run_patch.return_value = metric_output
        with self.assertRaises(trace_processor.InvalidTraceProcessorOutput):
          trace_processor.RunMetric(
              self.tp_path, '/path/to/proto', 'dummy_metric')

  def testRunMetricEmpty(self):
    metric_output = '{}'

    with mock.patch('core.tbmv3.trace_processor.METRICS_PATH', self.temp_dir):
      with mock.patch(RUN_METHOD) as run_patch:
          run_patch.return_value = metric_output
          # Checking that this doesn't throw errors.
          trace_processor.RunMetric(
              self.tp_path, '/path/to/proto', 'dummy_metric')

  def testRunMetricNoAnnotations(self):
    metric_output = '{"perfetto.protos.foo": {"bar": 42}}'

    with mock.patch('core.tbmv3.trace_processor.METRICS_PATH', self.temp_dir):
      with mock.patch(RUN_METHOD) as run_patch:
          run_patch.return_value = metric_output
          # Checking that this doesn't throw errors.
          trace_processor.RunMetric(
              self.tp_path, '/path/to/proto', 'dummy_metric')

  def testRunMetricFetchPowerProfile(self):
    FETCH_METHOD = (
        'core.tbmv3.trace_processor.binary_deps_manager.FetchDataFile')
    metric_output = '{}'

    with mock.patch(FETCH_METHOD) as fetch_patch:
      with mock.patch(RUN_METHOD) as run_patch:
          run_patch.return_value = metric_output
          # Checking that this doesn't throw errors.
          trace_processor.RunMetric(
              self.tp_path, '/path/to/proto', 'dummy_metric',
              fetch_power_profile=True)

    self.assertEqual(fetch_patch.call_count, 1)
    self.assertIn('--pre-metrics', run_patch.call_args[0])
