# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from unittest import mock

from core.results_processor import compute_metrics
from core.results_processor import testing

from tracing.mre import failure
from tracing.mre import job
from tracing.mre import mre_result
from tracing.value import histogram
from tracing.value import histogram_set


RUN_METRICS_METHOD = 'tracing.metrics.metric_runner.RunMetricOnSingleTrace'
GETSIZE_METHOD = 'os.path.getsize'
TRACE_PROCESSOR_METRIC_METHOD = 'core.tbmv3.trace_processor.RunMetrics'


class ComputeMetricsTest(unittest.TestCase):
  def testComputeTBMv2Metrics(self):
    test_result = testing.TestResult(
        'benchmark/story1',
        output_artifacts={
            compute_metrics.HTML_TRACE_NAME:
                testing.Artifact('/trace1.html', 'gs://trace1.html')},
        tags=['tbmv2:metric1'],
    )
    test_result['_histograms'] = histogram_set.HistogramSet()

    test_dict = histogram.Histogram('a', 'unitless').AsDict()
    metrics_result = mre_result.MreResult()
    metrics_result.AddPair('histograms', [test_dict])

    with mock.patch(GETSIZE_METHOD) as getsize_mock:
      with mock.patch(RUN_METRICS_METHOD) as run_metrics_mock:
        getsize_mock.return_value = 1000
        run_metrics_mock.return_value = metrics_result
        compute_metrics.ComputeTBMv2Metrics(test_result)

    histogram_dicts = test_result['_histograms'].AsDicts()
    self.assertEqual(histogram_dicts, [test_dict])
    self.assertEqual(test_result['status'], 'PASS')

  def testComputeTBMv2MetricsTraceTooBig(self):
    test_result = testing.TestResult(
        'benchmark/story1',
        output_artifacts={
            compute_metrics.HTML_TRACE_NAME:
                testing.Artifact('/trace1.html', 'gs://trace1.html')},
        tags=['tbmv2:metric1'],
    )
    test_result['_histograms'] = histogram_set.HistogramSet()

    with mock.patch(GETSIZE_METHOD) as getsize_mock:
      with mock.patch(RUN_METRICS_METHOD) as run_metrics_mock:
        getsize_mock.return_value = 1e9
        compute_metrics.ComputeTBMv2Metrics(test_result)
        self.assertEqual(run_metrics_mock.call_count, 0)

    histogram_dicts = test_result['_histograms'].AsDicts()
    self.assertEqual(histogram_dicts, [])
    self.assertEqual(test_result['status'], 'FAIL')
    self.assertFalse(test_result['expected'])

  def testComputeTBMv2MetricsFailure(self):
    test_result = testing.TestResult(
        'benchmark/story1',
        output_artifacts={
            compute_metrics.HTML_TRACE_NAME:
                testing.Artifact('/trace1.html', 'gs://trace1.html')},
        tags=['tbmv2:metric1'],
    )
    test_result['_histograms'] = histogram_set.HistogramSet()

    metrics_result = mre_result.MreResult()
    metrics_result.AddFailure(failure.Failure(job.Job(0), 0, 0, 0, 0, 0))

    with mock.patch(GETSIZE_METHOD) as getsize_mock:
      with mock.patch(RUN_METRICS_METHOD) as run_metrics_mock:
        getsize_mock.return_value = 100
        run_metrics_mock.return_value = metrics_result
        compute_metrics.ComputeTBMv2Metrics(test_result)

    histogram_dicts = test_result['_histograms'].AsDicts()
    self.assertEqual(histogram_dicts, [])
    self.assertEqual(test_result['status'], 'FAIL')
    self.assertFalse(test_result['expected'])

  def testComputeTBMv2MetricsSkipped(self):
    test_result = testing.TestResult(
        'benchmark/story1',
        output_artifacts={
            compute_metrics.HTML_TRACE_NAME:
                testing.Artifact('/trace1.html', 'gs://trace1.html')},
        tags=['tbmv2:metric1'],
        status='SKIP',
    )
    test_result['_histograms'] = histogram_set.HistogramSet()

    with mock.patch(RUN_METRICS_METHOD) as run_metrics_mock:
      compute_metrics.ComputeTBMv2Metrics(test_result)
      self.assertEqual(run_metrics_mock.call_count, 0)

    histogram_dicts = test_result['_histograms'].AsDicts()
    self.assertEqual(histogram_dicts, [])
    self.assertEqual(test_result['status'], 'SKIP')

  def testComputeTBMv3Metrics(self):
    test_result = testing.TestResult(
        'benchmark/story1',
        output_artifacts={
            compute_metrics.CONCATENATED_PROTO_NAME:
                testing.Artifact('/concatenated.pb')},
        tags=['tbmv3:metric'],
    )
    test_result['_histograms'] = histogram_set.HistogramSet()

    metric_result = histogram_set.HistogramSet()
    metric_result.CreateHistogram('a', 'unitless', [0])

    with mock.patch(TRACE_PROCESSOR_METRIC_METHOD) as run_metric_mock:
      run_metric_mock.return_value = metric_result
      compute_metrics.ComputeTBMv3Metrics(test_result, '/path/to/tp')

    histogram_dicts = test_result['_histograms'].AsDicts()
    self.assertEqual(histogram_dicts, metric_result.AsDicts())
    self.assertEqual(test_result['status'], 'PASS')
