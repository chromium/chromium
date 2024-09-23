# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for results_processor methods."""

import datetime
import os
import unittest
from unittest import mock

from core.results_processor import processor
from core.results_processor import testing

from tracing.value.diagnostics import generic_set
from tracing.value.diagnostics import date_range
from tracing.value import histogram_set


class ResultsProcessorUnitTests(unittest.TestCase):

  def testAddDiagnosticsToHistograms(self):
    start_ts = 1500000000
    start_iso = datetime.datetime.utcfromtimestamp(start_ts).isoformat() + 'Z'

    test_result = testing.TestResult(
        'benchmark/story',
        output_artifacts={
            'trace.html': testing.Artifact('/trace.html', 'gs://trace.html'),
        },
        start_time=start_iso,
        tags=['story_tag:test'],
        result_id='3',
    )
    test_result['_histograms'] = histogram_set.HistogramSet()
    test_result['_histograms'].CreateHistogram('a', 'unitless', [0])

    processor.AddDiagnosticsToHistograms(test_result,
                                         test_suite_start=start_iso,
                                         results_label='label',
                                         test_path_format='telemetry')

    hist = test_result['_histograms'].GetFirstHistogram()
    self.assertEqual(hist.diagnostics['labels'],
                     generic_set.GenericSet(['label']))
    self.assertEqual(hist.diagnostics['benchmarks'],
                     generic_set.GenericSet(['benchmark']))
    self.assertEqual(hist.diagnostics['benchmarkStart'],
                     date_range.DateRange(start_ts * 1e3))
    self.assertEqual(hist.diagnostics['traceStart'],
                     date_range.DateRange(start_ts * 1e3))
    self.assertEqual(hist.diagnostics['stories'],
                     generic_set.GenericSet(['story']))
    self.assertEqual(hist.diagnostics['storyTags'],
                     generic_set.GenericSet(['test']))
    self.assertEqual(hist.diagnostics['storysetRepeats'],
                     generic_set.GenericSet([3]))
    self.assertEqual(hist.diagnostics['traceUrls'],
                     generic_set.GenericSet(['gs://trace.html']))

  def testUploadArtifacts(self):
    test_result = testing.TestResult(
        'benchmark/story',
        output_artifacts={
            'logs': testing.Artifact('/log.log'),
            'trace.html': testing.Artifact('/trace.html'),
            'screenshot': testing.Artifact('/screenshot.png'),
        },
    )

    with mock.patch('py_utils.cloud_storage.Upload') as cloud_patch:
      cloud_patch.return_value = processor.cloud_storage.CloudFilepath(
          'bucket', 'path')
      processor.UploadArtifacts(test_result, 'bucket', 'run1')
      cloud_patch.assert_has_calls(
          [
              mock.call('bucket', 'run1/benchmark/story/retry_0/logs',
                        '/log.log'),
              mock.call('bucket', 'run1/benchmark/story/retry_0/trace.html',
                        '/trace.html'),
              mock.call('bucket', 'run1/benchmark/story/retry_0/screenshot',
                        '/screenshot.png'),
          ],
          any_order=True,
      )

    for artifact in test_result['outputArtifacts'].values():
      self.assertEqual(artifact['fetchUrl'], 'gs://bucket/path')
      self.assertEqual(artifact['viewUrl'],
                       'https://storage.cloud.google.com/bucket/path')

  def testRunIdentifier(self):
    with mock.patch('random.randint') as randint_patch:
      randint_patch.return_value = 54321
      run_identifier = processor.RunIdentifier(
          results_label='src@abc + 123',
          test_suite_start='2019-10-01T12:00:00.123456Z')
    self.assertEqual(run_identifier, 'src_abc_123_20191001T120000_54321')

  def testAggregateTBMv2Traces(self):
    test_result = testing.TestResult(
        'benchmark/story2',
        output_artifacts={
            'trace/1.json':
            testing.Artifact(
                os.path.join('test_run', 'story2', 'trace', '1.json')),
            'trace/2.txt':
            testing.Artifact(
                os.path.join('test_run', 'story2', 'trace', '2.txt')),
        },
    )

    serialize_method = 'tracing.trace_data.trace_data.SerializeAsHtml'
    with mock.patch(serialize_method) as mock_serialize:
      processor.AggregateTBMv2Traces(test_result)

    self.assertEqual(mock_serialize.call_count, 1)
    trace_files, file_path = mock_serialize.call_args[0][:2]
    self.assertEqual(
        set(trace_files),
        set([
            os.path.join('test_run', 'story2', 'trace', '1.json'),
            os.path.join('test_run', 'story2', 'trace', '2.txt'),
        ]),
    )
    self.assertEqual(
        file_path,
        os.path.join('test_run', 'story2', 'trace', 'trace.html'),
    )

    artifacts = test_result['outputArtifacts']
    self.assertEqual(len(artifacts), 1)
    self.assertEqual(list(artifacts.keys())[0], 'trace.html')

  def testMeasurementToHistogram(self):
    hist = processor.MeasurementToHistogram('a', {
        'unit': 'sizeInBytes',
        'samples': [1, 2, 3],
        'description': 'desc',
    })

    self.assertEqual(hist.name, 'a')
    self.assertEqual(hist.unit, 'sizeInBytes')
    self.assertEqual(hist.sample_values, [1, 2, 3])
    self.assertEqual(hist.description, 'desc')

  def testMeasurementToHistogramLegacyUnits(self):
    hist = processor.MeasurementToHistogram('a', {
        'unit': 'seconds',
        'samples': [1, 2, 3],
    })

    self.assertEqual(hist.name, 'a')
    self.assertEqual(hist.unit, 'ms_smallerIsBetter')
    self.assertEqual(hist.sample_values, [1000, 2000, 3000])

  def testMeasurementToHistogramUnknownUnits(self):
    with self.assertRaises(ValueError):
      processor.MeasurementToHistogram('a', {'unit': 'yards', 'samples': [9]})

  def testGetTraceUrlRemote(self):
    test_result = testing.TestResult(
        'benchmark/story',
        output_artifacts={
            'trace.html': testing.Artifact('trace.html', 'gs://trace.html')
        },
    )
    url = processor.GetTraceUrl(test_result)
    self.assertEqual(url, 'gs://trace.html')

  def testGetTraceUrlLocal(self):
    test_result = testing.TestResult(
        'benchmark/story',
        output_artifacts={'trace.html': testing.Artifact('trace.html')},
    )
    url = processor.GetTraceUrl(test_result)
    self.assertEqual(url, 'file://trace.html')

  def testGetTraceUrlEmpty(self):
    test_result = testing.TestResult('benchmark/story')
    url = processor.GetTraceUrl(test_result)
    self.assertIsNone(url)

  def testAmortizeProcessingDuration_UndefinedDuration(self):
    test_results = [testing.TestResult('benchmark/story')]
    del test_results[0]['runDuration']
    # pylint: disable=protected-access
    processor._AmortizeProcessingDuration(1.0, test_results)
    # pylint: enable=protected-access
    self.assertNotIn('runDuration', test_results[0])
    self.assertEqual(len(test_results), 1)

  def testAmortizeProcessingDuration_OneResult(self):
    test_results = [testing.TestResult('benchmark/story', run_duration='1.0s')]
    # pylint: disable=protected-access
    processor._AmortizeProcessingDuration(1.0, test_results)
    # pylint: enable=protected-access
    self.assertEqual(str(test_results[0]['runDuration']), '2.0s')
    self.assertEqual(len(test_results), 1)
