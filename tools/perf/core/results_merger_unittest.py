# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import unittest

from core import results_merger


class ResultMergerTest(unittest.TestCase):
  def setUp(self):
    self.sample_json_string = '''
    {
      "interrupted": false,
      "num_failures_by_type": {},
      "seconds_since_epoch": 10.0,
      "tests": {},
      "version": 3
    }
    '''

  def test_json_version_check_exception(self):
    json_string = '{"seconds_since_epoch": 1.0, "version": 2}'
    result = json.loads(json_string)
    with self.assertRaises(results_merger.MergeException) as c:
      results_merger.merge_test_results([result])
    self.assertTrue(
        'Unsupported version' in str(c.exception),
        'Version check failure message is not in exception. Exception: %s' %
        c.exception)

  def test_json_required_field_check_exception(self):
    json_string = '{"seconds_since_epoch": 1.0, "version": 3}'
    result = json.loads(json_string)
    with self.assertRaises(results_merger.MergeException) as c:
      results_merger.merge_test_results([result])
    self.assertTrue(
        'Invalid json test results' in str(c.exception),
        'Required key check failure message is not in exception. Exception: %s'
        % c.exception)

  def test_json_merge_tests(self):
    result_1 = json.loads(self.sample_json_string)
    result_2 = json.loads(self.sample_json_string)
    result_3 = json.loads(self.sample_json_string)
    result_1['tests'] = json.loads('''
    {
      "Benchmark-1": {
        "Story-1": {
          "actual": "PASS"
        },
        "Story-2": {
          "actual": "SKIP"
        }
      }
    }
    ''')
    result_2['tests'] = json.loads('''
    {
      "Benchmark-1": {
        "Story-3": {
          "actual": "FAIL"
        }
      },
      "Benchmark-2": {
        "Story-1": {
          "actual": "SKIP"
        }
      }
    }
    ''')
    result_3['tests'] = json.loads('''
    {
      "Benchmark-2": {
        "Story-2": {
          "actual": "PASS"
        }
      },
      "Benchmark-3": {
        "Story-1": {
          "actual": "PASS"
        }
      }
    }
    ''')
    merged_results = results_merger.merge_test_results(
        [result_1, result_2, result_3])
    self.assertEqual(len(merged_results['tests']), 3)
    self.assertEqual(len(merged_results['tests']['Benchmark-1']), 3)
    self.assertEqual(len(merged_results['tests']['Benchmark-2']), 2)
    self.assertEqual(len(merged_results['tests']['Benchmark-3']), 1)

  def test_json_merge_tests_non_dict_exception(self):
    result_1 = json.loads(self.sample_json_string)
    result_2 = json.loads(self.sample_json_string)
    result_1['tests'] = json.loads('''
      {
        "Benchmark-1": {
          "Story-1": {
            "actual": "PASS"
          }
        }
      }
      ''')
    result_2['tests'] = json.loads('''
      {
        "Benchmark-1": {
          "Story-1": {
            "actual": "FAIL"
          }
        }
      }
      ''')
    with self.assertRaises(results_merger.MergeException) as c:
      results_merger.merge_test_results([result_1, result_2])
    self.assertTrue(
        'not mergable' in str(c.exception),
        'Merge failure message is not in exception. Exception: %s' %
        c.exception)

  def test_json_merge_interrupted(self):
    result_1 = json.loads(self.sample_json_string)
    result_2 = json.loads(self.sample_json_string)
    result_2['interrupted'] = True
    merged_results = results_merger.merge_test_results([result_1, result_2])
    self.assertEqual(merged_results['interrupted'], True)

  def test_json_merge_seconds_since_epoch(self):
    result_1 = json.loads(self.sample_json_string)
    result_2 = json.loads(self.sample_json_string)
    result_2['seconds_since_epoch'] = 5.0
    merged_results = results_merger.merge_test_results([result_1, result_2])
    self.assertEqual(merged_results['seconds_since_epoch'], 5.0)

  def test_json_merge_nums(self):
    result_1 = json.loads(self.sample_json_string)
    result_2 = json.loads(self.sample_json_string)
    result_1['num_failures_by_type'] = json.loads('''
    {
      "PASS": 1,
      "SKIP": 5
    }
    ''')
    result_2['num_failures_by_type'] = json.loads('''
    {
      "PASS": 3,
      "FAIL": 2
    }
    ''')
    merged_results = results_merger.merge_test_results([result_1, result_2])
    self.assertEqual(merged_results['num_failures_by_type']['PASS'], 4)
    self.assertEqual(merged_results['num_failures_by_type']['SKIP'], 5)
    self.assertEqual(merged_results['num_failures_by_type']['FAIL'], 2)

  def test_json_merge_tests_cross_device(self):
    result_1 = json.loads(self.sample_json_string)
    result_2 = json.loads(self.sample_json_string)
    result_1['tests'] = json.loads('''
      {
        "Benchmark-1": {
          "Story-1": {
            "actual": "PASS PASS",
            "artifacts": {
              "logs.txt": [
                "123/1/logs.txt",
                "123/2/logs.txt"
              ],
              "trace.html": [
                "123/1/trace.html",
                "123/2/trace.html"
              ]
            },
            "expected": "PASS",
            "is_unexpected": false,
            "shard": 0,
            "time": 1.0,
            "times": [
              1.0,
              1.1
            ]
          }
        }
      }
      ''')
    result_2['tests'] = json.loads('''
      {
        "Benchmark-1": {
          "Story-1": {
            "actual": "FAIL PASS",
            "artifacts": {
              "logs.txt": [
                "456/1/logs.txt",
                "456/2/logs.txt"
              ],
              "screenshot.png": [
                "456/1/screenshot.png"
              ]
            },
            "expected": "PASS",
            "is_unexpected": true,
            "shard": 1,
            "time": 1.0,
            "times": [
              1.0,
              1.2
            ]
          }
        }
      }
      ''')
    merged_results = results_merger.merge_test_results([result_1, result_2],
                                                       True)
    self.assertEqual(len(merged_results['tests']), 1)
    self.assertEqual(len(merged_results['tests']['Benchmark-1']), 1)
    self.assertIn(
        'FAIL',
        merged_results['tests']['Benchmark-1']['Story-1']['actual'].split())
    self.assertIn(
        'PASS',
        merged_results['tests']['Benchmark-1']['Story-1']['actual'].split())
    self.assertEqual(
        4,
        len(merged_results['tests']['Benchmark-1']['Story-1']['artifacts']
            ['logs.txt']))
    self.assertEqual(
        2,
        len(merged_results['tests']['Benchmark-1']['Story-1']['artifacts']
            ['trace.html']))
    self.assertEqual(
        1,
        len(merged_results['tests']['Benchmark-1']['Story-1']['artifacts']
            ['screenshot.png']))
    self.assertEqual(
        4, len(merged_results['tests']['Benchmark-1']['Story-1']['times']))
    self.assertNotIn('shard', merged_results['tests']['Benchmark-1']['Story-1'])
    self.assertEqual(
        True,
        merged_results['tests']['Benchmark-1']['Story-1']['is_unexpected'])

  def test_json_merge_tests_cross_device_actual_pass(self):
    result_1 = json.loads(self.sample_json_string)
    result_2 = json.loads(self.sample_json_string)
    result_1['tests'] = json.loads('''
      {
        "Benchmark-1": {
          "Story-1": {
            "actual": "PASS",
            "expected": "PASS",
            "is_unexpected": false
          }
        }
      }
      ''')
    result_2['tests'] = json.loads('''
      {
        "Benchmark-1": {
          "Story-1": {
            "actual": "PASS",
            "expected": "PASS",
            "is_unexpected": false
          }
        }
      }
      ''')
    merged_results = results_merger.merge_test_results([result_1, result_2],
                                                       True)
    self.assertEqual(
        'PASS PASS',
        merged_results['tests']['Benchmark-1']['Story-1']['actual'])
    self.assertEqual(
        False,
        merged_results['tests']['Benchmark-1']['Story-1']['is_unexpected'])

  def test_json_merge_tests_cross_device_actual_fail(self):
    result_1 = json.loads(self.sample_json_string)
    result_2 = json.loads(self.sample_json_string)
    result_1['tests'] = json.loads('''
        {
          "Benchmark-1": {
            "Story-1": {
              "actual": "FAIL PASS PASS",
              "expected": "PASS",
              "is_unexpected": true
            }
          }
        }
        ''')
    result_2['tests'] = json.loads('''
        {
          "Benchmark-1": {
            "Story-1": {
              "actual": "PASS",
              "expected": "PASS",
              "is_unexpected": false
            }
          }
        }
        ''')
    merged_results = results_merger.merge_test_results([result_1, result_2],
                                                       True)
    self.assertIn('PASS',
                  merged_results['tests']['Benchmark-1']['Story-1']['actual'])
    self.assertIn('FAIL',
                  merged_results['tests']['Benchmark-1']['Story-1']['actual'])
    self.assertEqual(
        True,
        merged_results['tests']['Benchmark-1']['Story-1']['is_unexpected'])

  def test_json_merge_tests_cross_device_artifacts(self):
    result_1 = json.loads(self.sample_json_string)
    result_2 = json.loads(self.sample_json_string)
    result_1['tests'] = json.loads('''
        {
          "Benchmark-1": {
            "Story-1": {
              "actual": "PASS",
              "expected": "PASS",
              "artifacts": {
                "logs.txt": [
                  "123/1/logs.txt"
                ]
              }
            }
          }
        }
        ''')
    result_2['tests'] = json.loads('''
        {
          "Benchmark-1": {
            "Story-1": {
              "actual": "PASS",
              "expected": "PASS",
              "artifacts": {
                "logs.txt": [
                  "456/1/logs.txt"
                ],
                "trace.html": [
                  "123/1/trace.html"
                ]
              }
            }
          }
        }
        ''')
    merged_results = results_merger.merge_test_results([result_1, result_2],
                                                       True)
    self.assertEqual(
        2,
        len(merged_results['tests']['Benchmark-1']['Story-1']['artifacts']
            ['logs.txt']))
    self.assertEqual(
        1,
        len(merged_results['tests']['Benchmark-1']['Story-1']['artifacts']
            ['trace.html']))

  def test_json_merge_tests_cross_device_artifacts_missing(self):
    result_1 = json.loads(self.sample_json_string)
    result_2 = json.loads(self.sample_json_string)
    result_1['tests'] = json.loads('''
        {
          "Benchmark-1": {
            "Story-1": {
              "actual": "PASS",
              "expected": "PASS"
            }
          }
        }
        ''')
    result_2['tests'] = json.loads('''
        {
          "Benchmark-1": {
            "Story-1": {
              "actual": "PASS",
              "expected": "PASS",
              "artifacts": {
                "logs.txt": [
                  "456/1/logs.txt"
                ],
                "trace.html": [
                  "123/1/trace.html"
                ]
              }
            }
          }
        }
        ''')
    merged_results = results_merger.merge_test_results([result_1, result_2],
                                                       True)
    self.assertEqual(
        1,
        len(merged_results['tests']['Benchmark-1']['Story-1']['artifacts']
            ['logs.txt']))
    self.assertEqual(
        1,
        len(merged_results['tests']['Benchmark-1']['Story-1']['artifacts']
            ['trace.html']))

  def test_json_merge_tests_cross_device_times(self):
    result_1 = json.loads(self.sample_json_string)
    result_2 = json.loads(self.sample_json_string)
    result_1['tests'] = json.loads('''
        {
          "Benchmark-1": {
            "Story-1": {
              "actual": "PASS",
              "expected": "PASS",
              "time": 10.0,
              "times": [10.0, 15.0, 25.0]
            }
          }
        }
        ''')
    result_2['tests'] = json.loads('''
        {
          "Benchmark-1": {
            "Story-1": {
              "actual": "PASS",
              "expected": "PASS",
              "time": 20.0,
              "times": [20.0, 30.0]
            }
          }
        }
        ''')
    merged_results = results_merger.merge_test_results([result_1, result_2],
                                                       True)
    self.assertEqual(
        5, len(merged_results['tests']['Benchmark-1']['Story-1']['times']))
    self.assertEqual(10.0,
                     merged_results['tests']['Benchmark-1']['Story-1']['time'])

  def test_json_merge_tests_cross_device_times_missing(self):
    result_1 = json.loads(self.sample_json_string)
    result_2 = json.loads(self.sample_json_string)
    result_1['tests'] = json.loads('''
        {
          "Benchmark-1": {
            "Story-1": {
              "actual": "PASS",
              "expected": "PASS"
            }
          }
        }
        ''')
    result_2['tests'] = json.loads('''
        {
          "Benchmark-1": {
            "Story-1": {
              "actual": "PASS",
              "expected": "PASS",
              "time": 20.0,
              "times": [20.0, 30.0]
            }
          }
        }
        ''')
    merged_results = results_merger.merge_test_results([result_1, result_2],
                                                       True)
    self.assertEqual(
        2, len(merged_results['tests']['Benchmark-1']['Story-1']['times']))
    self.assertEqual(20.0,
                     merged_results['tests']['Benchmark-1']['Story-1']['time'])
