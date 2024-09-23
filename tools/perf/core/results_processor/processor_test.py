# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Integration tests for results_processor.

These tests write actual files with intermediate results, and run the
standalone results processor on them to check that output files are produced
with their expected contents.
"""

import csv
import datetime
import json
import os
import shutil
import tempfile
import unittest
from unittest import mock

from core.results_processor.formatters import csv_output
from core.results_processor.formatters import json3_output
from core.results_processor.formatters import histograms_output
from core.results_processor.formatters import html_output
from core.results_processor import compute_metrics
from core.results_processor import processor
from core.results_processor import testing

from tracing.value.diagnostics import generic_set
from tracing.value.diagnostics import date_range
from tracing.value import histogram
from tracing.value import histogram_set
from tracing_build import render_histograms_viewer


# For testing the TBMv2 workflow we use sampleMetric defined in
# third_party/catapult/tracing/tracing/metrics/sample_metric.html.
# This metric ignores the trace data and outputs a histogram with
# the following name and unit:
SAMPLE_HISTOGRAM_NAME = 'foo'
SAMPLE_HISTOGRAM_UNIT = 'sizeInBytes_smallerIsBetter'


class ResultsProcessorIntegrationTests(unittest.TestCase):

  def setUp(self):
    self.output_dir = tempfile.mkdtemp()
    self.intermediate_dir = os.path.join(self.output_dir, 'artifacts',
                                         'test_run')
    os.makedirs(self.intermediate_dir)

  def tearDown(self):
    shutil.rmtree(self.output_dir)

  def SerializeIntermediateResults(self, *test_results):
    testing.SerializeIntermediateResults(
        test_results, os.path.join(self.intermediate_dir,
                                   processor.TEST_RESULTS))

  def CreateHtmlTraceArtifact(self):
    """Create an empty file as a fake html trace."""
    with tempfile.NamedTemporaryFile(
        dir=self.intermediate_dir, delete=False) as artifact_file:
      pass
    return (compute_metrics.HTML_TRACE_NAME,
            testing.Artifact(artifact_file.name))

  def CreateProtoTraceArtifact(self):
    """Create an empty file as a fake proto trace."""
    with tempfile.NamedTemporaryFile(
        dir=self.intermediate_dir, delete=False) as artifact_file:
      pass
    return (compute_metrics.CONCATENATED_PROTO_NAME,
            testing.Artifact(artifact_file.name))

  def CreateDiagnosticsArtifact(self, **diagnostics):
    """Create an artifact with diagnostics."""
    with tempfile.NamedTemporaryFile(dir=self.intermediate_dir,
                                     delete=False,
                                     mode='w') as artifact_file:
      json.dump({'diagnostics': diagnostics}, artifact_file)
    return processor.DIAGNOSTICS_NAME, testing.Artifact(artifact_file.name)

  def CreateMeasurementsArtifact(self, measurements):
    with tempfile.NamedTemporaryFile(dir=self.intermediate_dir,
                                     delete=False,
                                     mode='w') as artifact_file:
      json.dump({'measurements': measurements}, artifact_file)
    return processor.MEASUREMENTS_NAME, testing.Artifact(artifact_file.name)

  def ReadSampleHistogramsFromCsv(self):
    with open(os.path.join(self.output_dir, csv_output.OUTPUT_FILENAME)) as f:
      # Filtering out rows with histograms other than SAMPLE_HISTOGRAM_NAME,
      # e.g. metrics_duration.
      return [
          row for row in csv.DictReader(f)
          if row['name'] == SAMPLE_HISTOGRAM_NAME
      ]

  def testJson3Output(self):
    self.SerializeIntermediateResults(
        testing.TestResult('benchmark/story',
                           run_duration='1.1s',
                           tags=['shard:7'],
                           start_time='2009-02-13T23:31:30.987000Z'),
        testing.TestResult('benchmark/story',
                           run_duration='1.2s',
                           tags=['shard:7']),
    )

    processor.main([
        '--is-unittest', '--output-format', 'json-test-results', '--output-dir',
        self.output_dir, '--intermediate-dir', self.intermediate_dir
    ])

    with open(os.path.join(self.output_dir, json3_output.OUTPUT_FILENAME)) as f:
      results = json.load(f)

    self.assertFalse(results['interrupted'])
    self.assertEqual(results['num_failures_by_type'], {'PASS': 2})
    self.assertEqual(results['seconds_since_epoch'], 1234567890.987)
    self.assertEqual(results['version'], 3)

    self.assertIn('benchmark', results['tests'])
    self.assertIn('story', results['tests']['benchmark'])
    test_result = results['tests']['benchmark']['story']

    self.assertEqual(test_result['actual'], 'PASS')
    self.assertEqual(test_result['expected'], 'PASS')
    # Amortization of processing time across test durations prevents us from
    # being exact here.
    self.assertGreaterEqual(test_result['times'][0], 1.1)
    self.assertGreaterEqual(test_result['times'][1], 1.2)
    self.assertEqual(len(test_result['times']), 2)
    self.assertGreaterEqual(test_result['time'], 1.1)
    self.assertEqual(test_result['shard'], 7)

  def testJson3OutputWithArtifacts(self):
    self.SerializeIntermediateResults(
        testing.TestResult('benchmark/story',
                           output_artifacts={
                               'logs':
                               testing.Artifact('/logs.txt',
                                                fetch_url='gs://logs.txt'),
                               'screenshot':
                               testing.Artifact(
                                   os.path.join(self.output_dir,
                                                'screenshot.png')),
                           }), )

    processor.main([
        '--is-unittest', '--output-format', 'json-test-results', '--output-dir',
        self.output_dir, '--intermediate-dir', self.intermediate_dir
    ])

    with open(os.path.join(self.output_dir, json3_output.OUTPUT_FILENAME)) as f:
      results = json.load(f)

    self.assertIn('benchmark', results['tests'])
    self.assertIn('story', results['tests']['benchmark'])
    self.assertIn('artifacts', results['tests']['benchmark']['story'])
    artifacts = results['tests']['benchmark']['story']['artifacts']

    self.assertEqual(len(artifacts), 2)
    self.assertEqual(artifacts['logs'], ['gs://logs.txt'])
    self.assertEqual(artifacts['screenshot'], ['screenshot.png'])

  def testMaxValuesPerTestCase(self):

    def SomeMeasurements(num):
      return self.CreateMeasurementsArtifact(
          {'n%d' % i: {
              'unit': 'count',
              'samples': [i]
          }
           for i in range(num)})

    self.SerializeIntermediateResults(
        testing.TestResult('benchmark/story1',
                           status='PASS',
                           output_artifacts=[SomeMeasurements(3)]),
        testing.TestResult('benchmark/story2',
                           status='PASS',
                           output_artifacts=[SomeMeasurements(7)]),
    )

    exit_code = processor.main([
        '--is-unittest', '--output-format', 'json-test-results',
        '--output-format', 'histograms', '--output-dir', self.output_dir,
        '--intermediate-dir', self.intermediate_dir,
        '--max-values-per-test-case', '5'
    ])
    self.assertEqual(exit_code, 1)

    with open(os.path.join(self.output_dir, json3_output.OUTPUT_FILENAME)) as f:
      results = json.load(f)

    self.assertEqual(results['tests']['benchmark']['story1']['actual'], 'PASS')
    self.assertEqual(results['tests']['benchmark']['story2']['actual'], 'FAIL')
    self.assertTrue(results['tests']['benchmark']['story2']['is_unexpected'])

  def testHistogramsOutput(self):
    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story',
            output_artifacts=[
                self.CreateHtmlTraceArtifact(),
                self.CreateDiagnosticsArtifact(
                    benchmarks=['benchmark'],
                    osNames=['linux'],
                    documentationUrls=[['documentation', 'url']])
            ],
            tags=['tbmv2:sampleMetric'],
            start_time='2009-02-13T23:31:30.987000Z',
        ), )

    with mock.patch('py_utils.cloud_storage.Upload') as cloud_patch:
      cloud_patch.return_value = processor.cloud_storage.CloudFilepath(
          bucket='bucket', remote_path='trace.html')
      processor.main([
          '--is-unittest',
          '--output-format',
          'histograms',
          '--output-dir',
          self.output_dir,
          '--intermediate-dir',
          self.intermediate_dir,
          '--results-label',
          'label',
          '--upload-results',
      ])

    with open(os.path.join(self.output_dir,
                           histograms_output.OUTPUT_FILENAME)) as f:
      results = json.load(f)

    out_histograms = histogram_set.HistogramSet()
    out_histograms.ImportDicts(results)

    hist = out_histograms.GetHistogramNamed(SAMPLE_HISTOGRAM_NAME)
    self.assertEqual(hist.unit, SAMPLE_HISTOGRAM_UNIT)

    self.assertEqual(hist.diagnostics['benchmarks'],
                     generic_set.GenericSet(['benchmark']))
    self.assertEqual(hist.diagnostics['osNames'],
                     generic_set.GenericSet(['linux']))
    self.assertEqual(hist.diagnostics['documentationUrls'],
                     generic_set.GenericSet([['documentation', 'url']]))
    self.assertEqual(hist.diagnostics['labels'],
                     generic_set.GenericSet(['label']))
    self.assertEqual(hist.diagnostics['benchmarkStart'],
                     date_range.DateRange(1234567890987))
    self.assertEqual(
        hist.diagnostics['traceUrls'],
        generic_set.GenericSet(
            ['https://storage.cloud.google.com/bucket/trace.html']))

  def testHistogramsOutputResetResults(self):
    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story',
            output_artifacts=[
                self.CreateHtmlTraceArtifact(),
            ],
            tags=['tbmv2:sampleMetric'],
        ), )

    processor.main([
        '--is-unittest',
        '--output-format',
        'histograms',
        '--output-dir',
        self.output_dir,
        '--intermediate-dir',
        self.intermediate_dir,
        '--results-label',
        'label1',
    ])

    processor.main([
        '--is-unittest',
        '--output-format',
        'histograms',
        '--output-dir',
        self.output_dir,
        '--intermediate-dir',
        self.intermediate_dir,
        '--results-label',
        'label2',
        '--reset-results',
    ])

    with open(os.path.join(self.output_dir,
                           histograms_output.OUTPUT_FILENAME)) as f:
      results = json.load(f)

    out_histograms = histogram_set.HistogramSet()
    out_histograms.ImportDicts(results)

    hist = out_histograms.GetHistogramNamed(SAMPLE_HISTOGRAM_NAME)
    self.assertEqual(hist.diagnostics['labels'],
                     generic_set.GenericSet(['label2']))

  def testHistogramsOutputAppendResults(self):
    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story',
            output_artifacts=[
                self.CreateHtmlTraceArtifact(),
            ],
            tags=['tbmv2:sampleMetric'],
        ), )

    processor.main([
        '--is-unittest',
        '--output-format',
        'histograms',
        '--output-dir',
        self.output_dir,
        '--intermediate-dir',
        self.intermediate_dir,
        '--results-label',
        'label1',
    ])

    processor.main([
        '--is-unittest',
        '--output-format',
        'histograms',
        '--output-dir',
        self.output_dir,
        '--intermediate-dir',
        self.intermediate_dir,
        '--results-label',
        'label2',
    ])

    with open(os.path.join(self.output_dir,
                           histograms_output.OUTPUT_FILENAME)) as f:
      results = json.load(f)

    out_histograms = histogram_set.HistogramSet()
    out_histograms.ImportDicts(results)

    sample_histograms = out_histograms.GetHistogramsNamed(SAMPLE_HISTOGRAM_NAME)
    self.assertEqual(len(sample_histograms), 2)

    expected_labels = set(['label1', 'label2'])
    observed_labels = set(label for hist in sample_histograms
                          for label in hist.diagnostics['labels'])
    self.assertEqual(observed_labels, expected_labels)

  def testHistogramsOutputNoAggregatedTrace(self):
    json_trace = os.path.join(self.output_dir, 'trace.json')
    with open(json_trace, 'w') as f:
      json.dump({'traceEvents': []}, f)

    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story',
            output_artifacts={'trace/trace.json': testing.Artifact(json_trace)},
            tags=['tbmv2:sampleMetric'],
        ), )

    processor.main([
        '--is-unittest',
        '--output-format',
        'histograms',
        '--output-dir',
        self.output_dir,
        '--intermediate-dir',
        self.intermediate_dir,
    ])

    with open(os.path.join(self.output_dir,
                           histograms_output.OUTPUT_FILENAME)) as f:
      results = json.load(f)

    out_histograms = histogram_set.HistogramSet()
    out_histograms.ImportDicts(results)

    hist = out_histograms.GetHistogramNamed(SAMPLE_HISTOGRAM_NAME)
    self.assertIsNotNone(hist)
    self.assertIn('traceUrls', hist.diagnostics)

  def testHistogramsOutputMeasurements(self):
    measurements = {
        'a': {
            'unit': 'ms',
            'samples': [4, 6],
            'description': 'desc_a'
        },
        'b': {
            'unit': 'ms',
            'samples': [5],
            'description': 'desc_b'
        },
    }
    start_ts = 1500000000
    start_iso = datetime.datetime.utcfromtimestamp(start_ts).isoformat() + 'Z'

    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story',
            output_artifacts=[
                self.CreateMeasurementsArtifact(measurements),
            ],
            tags=['story_tag:test'],
            start_time=start_iso,
        ), )

    processor.main([
        '--is-unittest',
        '--output-format',
        'histograms',
        '--output-dir',
        self.output_dir,
        '--intermediate-dir',
        self.intermediate_dir,
    ])

    with open(os.path.join(self.output_dir,
                           histograms_output.OUTPUT_FILENAME)) as f:
      results = json.load(f)

    out_histograms = histogram_set.HistogramSet()
    out_histograms.ImportDicts(results)
    self.assertEqual(len(out_histograms), 2)

    hist = out_histograms.GetHistogramNamed('a')
    self.assertEqual(hist.name, 'a')
    self.assertEqual(hist.unit, 'ms_smallerIsBetter')
    self.assertEqual(hist.sample_values, [4, 6])
    self.assertEqual(hist.description, 'desc_a')
    self.assertEqual(hist.diagnostics['benchmarks'],
                     generic_set.GenericSet(['benchmark']))
    self.assertEqual(hist.diagnostics['stories'],
                     generic_set.GenericSet(['story']))
    self.assertEqual(hist.diagnostics['storyTags'],
                     generic_set.GenericSet(['test']))
    self.assertEqual(hist.diagnostics['benchmarkStart'],
                     date_range.DateRange(start_ts * 1e3))

    hist = out_histograms.GetHistogramNamed('b')
    self.assertEqual(hist.name, 'b')
    self.assertEqual(hist.unit, 'ms_smallerIsBetter')
    self.assertEqual(hist.sample_values, [5])
    self.assertEqual(hist.description, 'desc_b')
    self.assertEqual(hist.diagnostics['benchmarks'],
                     generic_set.GenericSet(['benchmark']))
    self.assertEqual(hist.diagnostics['stories'],
                     generic_set.GenericSet(['story']))
    self.assertEqual(hist.diagnostics['storyTags'],
                     generic_set.GenericSet(['test']))
    self.assertEqual(hist.diagnostics['benchmarkStart'],
                     date_range.DateRange(start_ts * 1e3))

  def testHtmlOutput(self):
    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story',
            output_artifacts=[
                self.CreateHtmlTraceArtifact(),
                self.CreateDiagnosticsArtifact(
                    benchmarks=['benchmark'],
                    osNames=['linux'],
                    documentationUrls=[['documentation', 'url']]),
            ],
            tags=['tbmv2:sampleMetric'],
            start_time='2009-02-13T23:31:30.987000Z',
        ), )

    processor.main([
        '--is-unittest',
        '--output-format',
        'html',
        '--output-dir',
        self.output_dir,
        '--intermediate-dir',
        self.intermediate_dir,
        '--results-label',
        'label',
    ])

    with open(os.path.join(self.output_dir, html_output.OUTPUT_FILENAME)) as f:
      results = render_histograms_viewer.ReadExistingResults(f.read())

    out_histograms = histogram_set.HistogramSet()
    out_histograms.ImportDicts(results)

    hist = out_histograms.GetHistogramNamed(SAMPLE_HISTOGRAM_NAME)
    self.assertEqual(hist.unit, SAMPLE_HISTOGRAM_UNIT)

    self.assertEqual(hist.diagnostics['benchmarks'],
                     generic_set.GenericSet(['benchmark']))
    self.assertEqual(hist.diagnostics['osNames'],
                     generic_set.GenericSet(['linux']))
    self.assertEqual(hist.diagnostics['documentationUrls'],
                     generic_set.GenericSet([['documentation', 'url']]))
    self.assertEqual(hist.diagnostics['labels'],
                     generic_set.GenericSet(['label']))
    self.assertEqual(hist.diagnostics['benchmarkStart'],
                     date_range.DateRange(1234567890987))

  def testHtmlOutputResetResults(self):
    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story',
            output_artifacts=[
                self.CreateHtmlTraceArtifact(),
            ],
            tags=['tbmv2:sampleMetric'],
        ), )

    processor.main([
        '--is-unittest',
        '--output-format',
        'html',
        '--output-dir',
        self.output_dir,
        '--intermediate-dir',
        self.intermediate_dir,
        '--results-label',
        'label1',
    ])

    processor.main([
        '--is-unittest',
        '--output-format',
        'html',
        '--output-dir',
        self.output_dir,
        '--intermediate-dir',
        self.intermediate_dir,
        '--results-label',
        'label2',
        '--reset-results',
    ])

    with open(os.path.join(self.output_dir, html_output.OUTPUT_FILENAME)) as f:
      results = render_histograms_viewer.ReadExistingResults(f.read())

    out_histograms = histogram_set.HistogramSet()
    out_histograms.ImportDicts(results)

    hist = out_histograms.GetHistogramNamed(SAMPLE_HISTOGRAM_NAME)
    self.assertEqual(hist.diagnostics['labels'],
                     generic_set.GenericSet(['label2']))

  def testHtmlOutputAppendResults(self):
    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story',
            output_artifacts=[
                self.CreateHtmlTraceArtifact(),
            ],
            tags=['tbmv2:sampleMetric'],
        ), )

    processor.main([
        '--is-unittest',
        '--output-format',
        'html',
        '--output-dir',
        self.output_dir,
        '--intermediate-dir',
        self.intermediate_dir,
        '--results-label',
        'label1',
    ])

    processor.main([
        '--is-unittest',
        '--output-format',
        'html',
        '--output-dir',
        self.output_dir,
        '--intermediate-dir',
        self.intermediate_dir,
        '--results-label',
        'label2',
    ])

    with open(os.path.join(self.output_dir, html_output.OUTPUT_FILENAME)) as f:
      results = render_histograms_viewer.ReadExistingResults(f.read())

    out_histograms = histogram_set.HistogramSet()
    out_histograms.ImportDicts(results)
    sample_histograms = out_histograms.GetHistogramsNamed(SAMPLE_HISTOGRAM_NAME)
    self.assertEqual(len(sample_histograms), 2)

    expected_labels = set(['label1', 'label2'])
    observed_labels = set(label for hist in sample_histograms
                          for label in hist.diagnostics['labels'])
    self.assertEqual(observed_labels, expected_labels)

  def testCsvOutput(self):
    test_hist = histogram.Histogram('a', 'ms')
    test_hist.AddSample(3000)
    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story',
            output_artifacts=[
                self.CreateHtmlTraceArtifact(),
                self.CreateDiagnosticsArtifact(
                    benchmarks=['benchmark'],
                    osNames=['linux'],
                    documentationUrls=[['documentation', 'url']]),
            ],
            tags=['tbmv2:sampleMetric'],
            start_time='2009-02-13T23:31:30.987000Z',
        ), )

    processor.main([
        '--is-unittest',
        '--output-format',
        'csv',
        '--output-dir',
        self.output_dir,
        '--intermediate-dir',
        self.intermediate_dir,
        '--results-label',
        'label',
    ])

    sample_rows = self.ReadSampleHistogramsFromCsv()
    self.assertEqual(len(sample_rows), 1)

    actual = sample_rows[0]
    self.assertEqual(actual['name'], SAMPLE_HISTOGRAM_NAME)
    self.assertEqual(actual['unit'], 'B')
    self.assertEqual(actual['avg'], '50')
    self.assertEqual(actual['count'], '2')
    self.assertEqual(actual['benchmarks'], 'benchmark')
    self.assertEqual(actual['benchmarkStart'], '2009-02-13 23:31:30')
    self.assertEqual(actual['displayLabel'], 'label')
    self.assertEqual(actual['osNames'], 'linux')
    self.assertEqual(actual['traceStart'], '2009-02-13 23:31:30')

  def testCsvOutputResetResults(self):
    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story',
            output_artifacts=[
                self.CreateHtmlTraceArtifact(),
            ],
            tags=['tbmv2:sampleMetric'],
        ), )

    processor.main([
        '--is-unittest',
        '--output-format',
        'csv',
        '--output-dir',
        self.output_dir,
        '--intermediate-dir',
        self.intermediate_dir,
        '--results-label',
        'label1',
    ])

    processor.main([
        '--is-unittest',
        '--output-format',
        'csv',
        '--output-dir',
        self.output_dir,
        '--intermediate-dir',
        self.intermediate_dir,
        '--results-label',
        'label2',
        '--reset-results',
    ])

    sample_rows = self.ReadSampleHistogramsFromCsv()
    self.assertEqual(len(sample_rows), 1)
    self.assertEqual(sample_rows[0]['displayLabel'], 'label2')

  def testCsvOutputAppendResults(self):
    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story',
            output_artifacts=[
                self.CreateHtmlTraceArtifact(),
            ],
            tags=['tbmv2:sampleMetric'],
        ), )

    processor.main([
        '--is-unittest',
        '--output-format',
        'csv',
        '--output-dir',
        self.output_dir,
        '--intermediate-dir',
        self.intermediate_dir,
        '--results-label',
        'label1',
    ])

    processor.main([
        '--is-unittest',
        '--output-format',
        'csv',
        '--output-dir',
        self.output_dir,
        '--intermediate-dir',
        self.intermediate_dir,
        '--results-label',
        'label2',
    ])

    sample_rows = self.ReadSampleHistogramsFromCsv()
    self.assertEqual(len(sample_rows), 2)
    self.assertEqual(sample_rows[0]['displayLabel'], 'label2')
    self.assertEqual(sample_rows[1]['displayLabel'], 'label1')

  def testExitCodeHasFailures(self):
    self.SerializeIntermediateResults(
        testing.TestResult('benchmark/story', status='PASS'),
        testing.TestResult('benchmark/story', status='FAIL'),
    )

    exit_code = processor.main([
        '--is-unittest', '--output-format', 'json-test-results', '--output-dir',
        self.output_dir, '--intermediate-dir', self.intermediate_dir
    ])

    self.assertEqual(exit_code, 1)

  def testExitCodeAllSkipped(self):
    self.SerializeIntermediateResults(
        testing.TestResult('benchmark/story', status='SKIP'),
        testing.TestResult('benchmark/story', status='SKIP'),
    )

    exit_code = processor.main([
        '--is-unittest', '--output-format', 'json-test-results', '--output-dir',
        self.output_dir, '--intermediate-dir', self.intermediate_dir
    ])

    self.assertEqual(exit_code, 111)

  def testExitCodeSomeSkipped(self):
    self.SerializeIntermediateResults(
        testing.TestResult('benchmark/story', status='SKIP'),
        testing.TestResult('benchmark/story', status='PASS'),
    )

    exit_code = processor.main([
        '--is-unittest', '--output-format', 'json-test-results', '--output-dir',
        self.output_dir, '--intermediate-dir', self.intermediate_dir
    ])

    self.assertEqual(exit_code, 0)

  def testHistogramsOutput_TBMv3(self):
    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story',
            output_artifacts=[
                self.CreateProtoTraceArtifact(),
                self.CreateDiagnosticsArtifact(
                    benchmarks=['benchmark'],
                    osNames=['linux'],
                    documentationUrls=[['documentation', 'url']])
            ],
            tags=['tbmv3:dummy_metric'],
            start_time='2009-02-13T23:31:30.987000Z',
        ), )

    processor.main([
        '--is-unittest',
        '--output-format',
        'histograms',
        '--output-dir',
        self.output_dir,
        '--intermediate-dir',
        self.intermediate_dir,
        '--results-label',
        'label',
        '--experimental-tbmv3-metrics',
    ])

    with open(os.path.join(self.output_dir,
                           histograms_output.OUTPUT_FILENAME)) as f:
      results = json.load(f)

    out_histograms = histogram_set.HistogramSet()
    out_histograms.ImportDicts(results)

    # For testing the TBMv3 workflow we use dummy_metric defined in
    # tools/perf/core/tbmv3/metrics/dummy_metric_*.
    hist = out_histograms.GetHistogramNamed('dummy::simple_field')
    self.assertEqual(hist.unit, 'count_smallerIsBetter')

    self.assertEqual(hist.diagnostics['benchmarks'],
                     generic_set.GenericSet(['benchmark']))
    self.assertEqual(hist.diagnostics['osNames'],
                     generic_set.GenericSet(['linux']))
    self.assertEqual(hist.diagnostics['documentationUrls'],
                     generic_set.GenericSet([['documentation', 'url']]))
    self.assertEqual(hist.diagnostics['labels'],
                     generic_set.GenericSet(['label']))
    self.assertEqual(hist.diagnostics['benchmarkStart'],
                     date_range.DateRange(1234567890987))

  def testComplexMetricOutput_TBMv3(self):
    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story',
            output_artifacts=[
                self.CreateProtoTraceArtifact(),
                self.CreateDiagnosticsArtifact(
                    benchmarks=['benchmark'],
                    osNames=['linux'],
                    documentationUrls=[['documentation', 'url']])
            ],
            tags=['tbmv3:dummy_metric'],
            start_time='2009-02-13T23:31:30.987000Z',
        ), )

    processor.main([
        '--is-unittest',
        '--output-format',
        'histograms',
        '--output-dir',
        self.output_dir,
        '--intermediate-dir',
        self.intermediate_dir,
        '--results-label',
        'label',
        '--experimental-tbmv3-metrics',
    ])

    with open(os.path.join(self.output_dir,
                           histograms_output.OUTPUT_FILENAME)) as f:
      results = json.load(f)

    # For testing the TBMv3 workflow we use dummy_metric defined in
    # tools/perf/core/tbmv3/metrics/dummy_metric_*.
    out_histograms = histogram_set.HistogramSet()
    out_histograms.ImportDicts(results)

    simple_field = out_histograms.GetHistogramNamed('dummy::simple_field')
    self.assertEqual(simple_field.unit, 'count_smallerIsBetter')
    self.assertEqual((simple_field.num_values, simple_field.average), (1, 42))

    repeated_field = out_histograms.GetHistogramNamed('dummy::repeated_field')
    self.assertEqual(repeated_field.unit, 'ms_biggerIsBetter')
    self.assertEqual(repeated_field.num_values, 3)
    self.assertEqual(repeated_field.sample_values, [1, 2, 3])

    # Unannotated fields should not be included in final histogram output.
    simple_nested_unannotated = out_histograms.GetHistogramsNamed(
        'dummy::simple_nested:unannotated_field')
    self.assertEqual(len(simple_nested_unannotated), 0)
    repeated_nested_unannotated = out_histograms.GetHistogramsNamed(
        'dummy::repeated_nested:unannotated_field')
    self.assertEqual(len(repeated_nested_unannotated), 0)

    simple_nested_annotated = out_histograms.GetHistogramNamed(
        'dummy::simple_nested:annotated_field')
    self.assertEqual(simple_nested_annotated.unit, 'ms_smallerIsBetter')
    self.assertEqual(simple_nested_annotated.num_values, 1)
    self.assertEqual(simple_nested_annotated.average, 44)
    repeated_nested_annotated = out_histograms.GetHistogramNamed(
        'dummy::repeated_nested:annotated_field')
    self.assertEqual(repeated_nested_annotated.unit, 'ms_smallerIsBetter')
    self.assertEqual(repeated_nested_annotated.num_values, 2)
    self.assertEqual(repeated_nested_annotated.sample_values, [2, 4])

  def testExtraMetrics(self):
    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story',
            output_artifacts=[
                self.CreateHtmlTraceArtifact(),
                self.CreateProtoTraceArtifact(),
            ],
            tags=['tbmv2:sampleMetric'],
            start_time='2009-02-13T23:31:30.987000Z',
        ), )

    processor.main([
        '--is-unittest',
        '--output-format',
        'histograms',
        '--output-dir',
        self.output_dir,
        '--intermediate-dir',
        self.intermediate_dir,
        '--results-label',
        'label',
        '--experimental-tbmv3-metrics',
        '--extra-metric',
        'tbmv3:dummy_metric',
    ])

    with open(os.path.join(self.output_dir,
                           histograms_output.OUTPUT_FILENAME)) as f:
      results = json.load(f)

    out_histograms = histogram_set.HistogramSet()
    out_histograms.ImportDicts(results)

    # Both sampleMetric and dummy_metric should have been computed.
    hist1 = out_histograms.GetHistogramNamed(SAMPLE_HISTOGRAM_NAME)
    self.assertEqual(hist1.unit, SAMPLE_HISTOGRAM_UNIT)
    hist2 = out_histograms.GetHistogramNamed('dummy::simple_field')
    self.assertEqual(hist2.unit, 'count_smallerIsBetter')

  def testMultipleTBMv3Metrics(self):
    self.SerializeIntermediateResults(
        testing.TestResult(
            'benchmark/story',
            output_artifacts=[
                self.CreateProtoTraceArtifact(),
                self.CreateDiagnosticsArtifact(
                    benchmarks=['benchmark'],
                    osNames=['linux'],
                    documentationUrls=[['documentation', 'url']])
            ],
            tags=['tbmv3:dummy_metric', 'tbmv3:test_chrome_metric'],
            start_time='2009-02-13T23:31:30.987000Z',
        ), )

    processor.main([
        '--is-unittest',
        '--output-format',
        'histograms',
        '--output-dir',
        self.output_dir,
        '--intermediate-dir',
        self.intermediate_dir,
        '--results-label',
        'label',
        '--experimental-tbmv3-metrics',
    ])

    with open(os.path.join(self.output_dir,
                           histograms_output.OUTPUT_FILENAME)) as f:
      results = json.load(f)

    out_histograms = histogram_set.HistogramSet()
    out_histograms.ImportDicts(results)

    # We use two metrics for testing here. The dummy_metric is defined in
    # tools/perf/core/tbmv3/metrics/dummy_metric_*.
    # The test_chrome_metric is built into trace_processor, see source in
    # third_party/perfetto/src/trace_processor/metrics/chrome/
    # test_chrome_metric.sql.
    hist1 = out_histograms.GetHistogramNamed('dummy::simple_field')
    self.assertEqual(hist1.sample_values, [42])

    hist2 = out_histograms.GetHistogramNamed('test_chrome::test_value')
    self.assertEqual(hist2.sample_values, [1])
