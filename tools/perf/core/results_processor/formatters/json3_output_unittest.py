# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import unittest

from core.results_processor.formatters import json3_output
from core.results_processor import testing


class Json3OutputTest(unittest.TestCase):
  def setUp(self):
    self.base_dir = 'base_dir'
    self.test_path_format = 'telemetry'

  def Convert(self, test_results):
    test_results_copy = copy.deepcopy(test_results)
    results = json3_output.Convert(
        test_results_copy, self.base_dir, self.test_path_format)
    # Convert should not modify the original intermediate results.
    self.assertEqual(test_results_copy, test_results)
    return results

  def FindTestResult(self, results, benchmark, story):
    node = results['tests']
    for key in (benchmark, story):
      self.assertIn(key, node)
      node = node[key]
    return node

  def testStartTime(self):
    results = self.Convert([
        testing.TestResult('benchmark/story',
                           start_time='2009-02-13T23:31:30.987000Z')
    ])

    self.assertFalse(results['interrupted'])
    self.assertEqual(results['path_delimiter'], '/')
    self.assertEqual(results['seconds_since_epoch'], 1234567890.987)
    self.assertEqual(results['version'], 3)

  def testSingleTestCase(self):
    results = self.Convert([
        testing.TestResult('benchmark/story', run_duration='1.2s')
    ])

    test_result = self.FindTestResult(results, 'benchmark', 'story')
    self.assertEqual(test_result['actual'], 'PASS')
    self.assertEqual(test_result['expected'], 'PASS')
    self.assertEqual(test_result['times'], [1.2])
    self.assertEqual(test_result['time'], 1.2)
    self.assertNotIn('shard', test_result)
    self.assertEqual(results['num_failures_by_type'], {'PASS': 1})

  # TODO(crbug.com/40636038): Remove this test when all stories have
  # url-friendly names without special characters.
  def testUrlAsStoryName(self):
    results = self.Convert([
        testing.TestResult('benchmark/http://example.com')
    ])

    test_result = self.FindTestResult(
        results, 'benchmark', 'http://example.com')
    self.assertEqual(test_result['actual'], 'PASS')

  def testTwoTestCases(self):
    results = self.Convert([
        testing.TestResult('benchmark/story1', tags=['shard:7']),
        testing.TestResult('benchmark/story2', tags=['shard:3'])
    ])

    test_result = self.FindTestResult(results, 'benchmark', 'story1')
    self.assertEqual(test_result['actual'], 'PASS')
    self.assertEqual(test_result['expected'], 'PASS')
    self.assertEqual(test_result['shard'], 7)

    test_result = self.FindTestResult(results, 'benchmark', 'story2')
    self.assertEqual(test_result['actual'], 'PASS')
    self.assertEqual(test_result['expected'], 'PASS')
    self.assertEqual(test_result['shard'], 3)

    self.assertEqual(results['num_failures_by_type'], {'PASS': 2})

  def testRepeatedTestCases(self):
    results = self.Convert([
        testing.TestResult('benchmark/story1', status='PASS',
                           run_duration='1.2s'),
        testing.TestResult('benchmark/story2', status='SKIP'),
        testing.TestResult('benchmark/story1', status='PASS',
                           run_duration='3.4s'),
        testing.TestResult('benchmark/story2', status='SKIP'),
    ])

    test_result = self.FindTestResult(results, 'benchmark', 'story1')
    self.assertEqual(test_result['actual'], 'PASS')
    self.assertEqual(test_result['expected'], 'PASS')
    self.assertEqual(test_result['times'], [1.2, 3.4])
    self.assertEqual(test_result['time'], 1.2)

    test_result = self.FindTestResult(results, 'benchmark', 'story2')
    self.assertEqual(test_result['actual'], 'SKIP')
    self.assertEqual(test_result['expected'], 'SKIP')

    self.assertEqual(results['num_failures_by_type'], {'PASS': 2, 'SKIP': 2})

  def testFailedAndSkippedTestCases(self):
    results = self.Convert([
        testing.TestResult('benchmark/story1', status='PASS'),
        testing.TestResult('benchmark/story2', status='PASS'),
        testing.TestResult('benchmark/story1', status='FAIL'),
        testing.TestResult('benchmark/story2', status='SKIP',
                           expected=False),
    ])

    test_result = self.FindTestResult(results, 'benchmark', 'story1')
    self.assertEqual(test_result['actual'], 'FAIL')
    self.assertEqual(test_result['expected'], 'PASS')
    self.assertTrue(test_result['is_unexpected'])

    test_result = self.FindTestResult(results, 'benchmark', 'story2')
    self.assertEqual(test_result['actual'], 'PASS SKIP')
    self.assertEqual(test_result['expected'], 'PASS')
    self.assertTrue(test_result['is_unexpected'])

    self.assertEqual(results['num_failures_by_type'],
                     {'PASS': 2, 'SKIP': 1, 'FAIL': 1})

  def testDedupedStatus(self):
    results = self.Convert([
        testing.TestResult('benchmark/story1', status='PASS'),
        testing.TestResult('benchmark/story2', status='SKIP'),
        testing.TestResult('benchmark/story3', status='FAIL'),
        testing.TestResult('benchmark/story1', status='PASS'),
        testing.TestResult('benchmark/story2', status='SKIP'),
        testing.TestResult('benchmark/story3', status='FAIL'),
    ])

    test_result = self.FindTestResult(results, 'benchmark', 'story1')
    self.assertEqual(test_result['actual'], 'PASS')
    self.assertEqual(test_result['expected'], 'PASS')
    self.assertFalse(test_result['is_unexpected'])

    test_result = self.FindTestResult(results, 'benchmark', 'story2')
    self.assertEqual(test_result['actual'], 'SKIP')
    self.assertEqual(test_result['expected'], 'SKIP')
    self.assertFalse(test_result['is_unexpected'])

    test_result = self.FindTestResult(results, 'benchmark', 'story3')
    self.assertEqual(test_result['actual'], 'FAIL')
    self.assertEqual(test_result['expected'], 'PASS')
    self.assertTrue(test_result['is_unexpected'])

    self.assertEqual(results['num_failures_by_type'],
                     {'PASS': 2, 'SKIP': 2, 'FAIL': 2})

  def testRepeatedTestCaseWithArtifacts(self):
    self.base_dir = 'base'
    results = self.Convert([
        testing.TestResult('benchmark/story1', output_artifacts={
            'logs.txt': testing.Artifact('base/artifacts/logs1.txt')
        }),
        testing.TestResult('benchmark/story1', output_artifacts={
            'logs.txt': testing.Artifact('base/artifacts/logs2.txt'),
            'trace.json': testing.Artifact('base/artifacts/trace2.json')
        }),
    ])

    test_result = self.FindTestResult(results, 'benchmark', 'story1')
    self.assertEqual(test_result['actual'], 'PASS')
    self.assertEqual(test_result['expected'], 'PASS')
    self.assertEqual(test_result['artifacts'], {
        'logs.txt': ['artifacts/logs1.txt', 'artifacts/logs2.txt'],
        'trace.json': ['artifacts/trace2.json']
    })

  def testRemoteArtifacts(self):
    results = self.Convert([
        testing.TestResult('benchmark/story1', output_artifacts={
            'logs.txt': testing.Artifact(
                'base/artifacts/logs1.txt',
                fetch_url='gs://artifacts/logs1.txt')
        }),
        testing.TestResult('benchmark/story1', output_artifacts={
            'logs.txt': testing.Artifact(
                'base/artifacts/logs2.txt',
                fetch_url='gs://artifacts/logs2.txt'),
            'trace.json': testing.Artifact(
                'base/artifacts/trace2.json',
                fetch_url='gs://artifacts/trace2.json')
        }),
    ])

    test_result = self.FindTestResult(results, 'benchmark', 'story1')
    self.assertEqual(test_result['actual'], 'PASS')
    self.assertEqual(test_result['expected'], 'PASS')
    self.assertEqual(test_result['artifacts'], {
        'logs.txt': [
            'gs://artifacts/logs1.txt',
            'gs://artifacts/logs2.txt'
        ],
        'trace.json': [
            'gs://artifacts/trace2.json'
        ]
    })
