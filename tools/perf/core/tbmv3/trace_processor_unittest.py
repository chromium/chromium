# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import tempfile
import unittest
from unittest import mock

from core.tbmv3 import trace_processor

RUN_METHOD = 'core.tbmv3.trace_processor._RunTraceProcessor'


class TraceProcessorTestCase(unittest.TestCase):

  def setUp(self):
    self.temp_dir = tempfile.mkdtemp()
    self.trace_path = self.CreateEmptyProtoTrace()
    self.tp_path = os.path.join(self.temp_dir, 'trace_processor_shell')
    with open(self.tp_path, 'w'):
      pass
    with open(os.path.join(self.temp_dir, 'dummy_metric.sql'), 'w'):
      pass
    with open(os.path.join(self.temp_dir, 'dummy_metric.proto'), 'w'):
      pass

  def tearDown(self):
    shutil.rmtree(self.temp_dir)

  def CreateEmptyProtoTrace(self):
    """Create an empty file as a proto trace."""
    with tempfile.NamedTemporaryFile(dir=self.temp_dir,
                                     delete=False) as trace_file:
      # Open temp file and close it so it's written to disk.
      pass
    return trace_file.name

  def testConvertProtoTraceToJson(self):
    with mock.patch(RUN_METHOD):
      trace_processor.ConvertProtoTraceToJson(self.tp_path, '/path/to/proto',
                                              '/path/to/json')

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
        histograms = trace_processor.RunMetric(self.tp_path, '/path/to/proto',
                                               'dummy_metric')

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
        histograms = trace_processor.RunMetric(self.tp_path, '/path/to/proto',
                                               'dummy_metric')

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
          trace_processor.RunMetric(self.tp_path, '/path/to/proto',
                                    'dummy_metric')

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
          trace_processor.RunMetric(self.tp_path, '/path/to/proto',
                                    'dummy_metric')

  def testRunMetricEmpty(self):
    metric_output = '{}'

    with mock.patch('core.tbmv3.trace_processor.METRICS_PATH', self.temp_dir):
      with mock.patch(RUN_METHOD) as run_patch:
        run_patch.return_value = metric_output
        # Checking that this doesn't throw errors.
        trace_processor.RunMetric(self.tp_path, '/path/to/proto',
                                  'dummy_metric')

  def testRunMetricNoAnnotations(self):
    metric_output = '{"perfetto.protos.foo": {"bar": 42}}'

    with mock.patch('core.tbmv3.trace_processor.METRICS_PATH', self.temp_dir):
      with mock.patch(RUN_METHOD) as run_patch:
        run_patch.return_value = metric_output
        # Checking that this doesn't throw errors.
        trace_processor.RunMetric(self.tp_path, '/path/to/proto',
                                  'dummy_metric')

  def testRunMetricFetchPowerProfile(self):
    FETCH_METHOD = (
        'core.tbmv3.trace_processor.binary_deps_manager.FetchDataFile')
    metric_output = '{}'

    with mock.patch(FETCH_METHOD) as fetch_patch:
      with mock.patch(RUN_METHOD) as run_patch:
        run_patch.return_value = metric_output
        # Checking that this doesn't throw errors.
        trace_processor.RunMetric(self.tp_path,
                                  '/path/to/proto',
                                  'dummy_metric',
                                  fetch_power_profile=True)

    self.assertEqual(fetch_patch.call_count, 1)
    self.assertIn('--pre-metrics', run_patch.call_args[0])

  def testRunQueryEmptyInput(self):
    sql_query = 'SELECT name, ts FROM slice ORDER BY ts LIMIT 1'
    sql_output = ''

    with mock.patch('core.tbmv3.trace_processor.METRICS_PATH', self.temp_dir):
      with mock.patch(RUN_METHOD) as run_patch:
        run_patch.return_value = sql_output
        query_output = trace_processor.RunQuery(self.tp_path, '/path/to/proto',
                                                sql_query)

    expected_output = []
    self.assertEqual(query_output, expected_output)

  def testRunQueryGarbageInput(self):
    sql_query = 'SELECT name, ts FROM slice ORDER BY ts LIMIT 1'
    sql_output = """adsfsdfdsaf"""

    with mock.patch('core.tbmv3.trace_processor.METRICS_PATH', self.temp_dir):
      with mock.patch(RUN_METHOD) as run_patch:
        run_patch.return_value = sql_output
        query_output = trace_processor.RunQuery(self.tp_path, '/path/to/proto',
                                                sql_query)

    expected_output = []
    self.assertEqual(query_output, expected_output)

  def testRunQueryNoRows(self):
    sql_query = 'SELECT name, ts FROM slice ORDER BY ts LIMIT 1'
    sql_output = """"ts","ts+dur"\n\n\n\n\n\n\n\n\n\n\n\n"""

    with mock.patch('core.tbmv3.trace_processor.METRICS_PATH', self.temp_dir):
      with mock.patch(RUN_METHOD) as run_patch:
        run_patch.return_value = sql_output
        query_output = trace_processor.RunQuery(self.tp_path, '/path/to/proto',
                                                sql_query)

    expected_output = []
    self.assertEqual(query_output, expected_output)

  def testRunQueryMultipleRows(self):
    sql_query = 'SELECT name, ts FROM slice ORDER BY ts LIMIT 3'
    sql_output = '"name","ts"\n' +\
                '"foo",123\n' +\
                '"bar",468\n' +\
                '"bas",432\n'

    with mock.patch('core.tbmv3.trace_processor.METRICS_PATH', self.temp_dir):
      with mock.patch(RUN_METHOD) as run_patch:
        run_patch.return_value = sql_output
        query_output = trace_processor.RunQuery(self.tp_path, '/path/to/proto',
                                                sql_query)

    expected_output = [{
        'name': 'foo',
        'ts': '123'
    }, {
        'name': 'bar',
        'ts': '468'
    }, {
        'name': 'bas',
        'ts': '432'
    }]
    self.assertEqual(query_output, expected_output)

  def testRunQueryDatatypes(self):
    sql_query = 'SELECT name,arg_set_id,ts/100.0,track_id=2 FROM slice LIMIT 3'
    sql_output = '"name","arg_set_id","ts/100.0","track_id=2"\n' +\
                '"bar",3,59.82,0\n' +\
                '"foo",2,37.14,1\n' +\
                '"bas",5,92.01,0\n'

    with mock.patch('core.tbmv3.trace_processor.METRICS_PATH', self.temp_dir):
      with mock.patch(RUN_METHOD) as run_patch:
        run_patch.return_value = sql_output
        query_output = trace_processor.RunQuery(self.tp_path, '/path/to/proto',
                                                sql_query)

    expected_output = [{
        'track_id=2': '0',
        'ts/100.0': '59.82',
        'name': 'bar',
        'arg_set_id': '3'
    }, {
        'track_id=2': '1',
        'ts/100.0': '37.14',
        'name': 'foo',
        'arg_set_id': '2'
    }, {
        'track_id=2': '0',
        'ts/100.0': '92.01',
        'name': 'bas',
        'arg_set_id': '5'
    }]
    self.assertEqual(query_output, expected_output)

  def testRunQueryDatatypeNull(self):
    sql_query = 'SELECT int_value, str_value FROM metadata LIMIT 2'
    sql_output = '"int_value","str_value"\n' +\
                '"[NULL]","Darwin"\n' +\
                '"[NULL]","Linux"\n'

    with mock.patch('core.tbmv3.trace_processor.METRICS_PATH', self.temp_dir):
      with mock.patch(RUN_METHOD) as run_patch:
        run_patch.return_value = sql_output
        query_output = trace_processor.RunQuery(self.tp_path, '/path/to/proto',
                                                sql_query)

    expected_output = [{
        'int_value': None,
        'str_value': 'Darwin'
    }, {
        'int_value': None,
        'str_value': 'Linux'
    }]
    self.assertEqual(query_output, expected_output)

  def testRunQueryNoPatch(self):
    sql_query = 'SELECT int_value, str_value FROM metadata LIMIT 1'
    try:
      trace_processor.RunQuery(None, self.trace_path, sql_query)
    except Exception as error:
      self.fail('Unexpected trace_processor error: {}'.format(error))

  def testWithInterferingEnvironmentVariables(self):
    os.environ['PERFETTO_SYMBOLIZER_MODE'] = 'placeholder'
    os.environ['PERFETTO_BINARY_PATH'] = 'placeholder'

    sql_query = 'SELECT int_value, str_value FROM metadata LIMIT 1'
    try:
      trace_processor.RunQuery(None, self.trace_path, sql_query)
    except Exception as error:
      self.fail('Unexpected trace_processor error: {}'.format(error))
    finally:
      os.environ.pop('PERFETTO_BINARY_PATH', None)
      os.environ.pop('PERFETTO_SYMBOLIZER_MODE', None)
