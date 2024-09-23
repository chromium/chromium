# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import copy
import json
import os
import tempfile
import unittest
from unittest import mock

import six

# This is necessary because io.StringIO in Python 2 does not accept str, only
# unicode. BytesIO works in Python 2, but then complains when given a str
# instead of bytes in Python 3.
if six.PY2:
  from cStringIO import StringIO  # pylint: disable=wrong-import-order,import-error
else:
  from io import StringIO  # pylint: disable=wrong-import-order

from core import perf_data_generator
from core.perf_data_generator import BenchmarkMetadata


class PerfDataGeneratorTest(unittest.TestCase):
  def setUp(self):
    # Test config can be big, so set maxDiff to None to see the full comparision
    # diff when assertEqual fails.
    self.maxDiff = None

  def test_get_scheduled_non_telemetry_benchmarks(self):
    tmpfile = tempfile.NamedTemporaryFile(delete=False)
    tmpfile.close()
    fake_perf_waterfall_file = tmpfile.name

    data = {
        'builder 1': {
          'isolated_scripts': [
            {'name': 'test_dancing'},
            {'name': 'test_singing'},
            {'name': 'performance_test_suite'},
          ],
          'scripts': [
            {'name': 'ninja_test'},
          ]
        },
        'builder 2': {
          'scripts': [
            {'name': 'gun_slinger'},
          ]
        }
      }
    try:
      with open(fake_perf_waterfall_file, 'w') as f:
        json.dump(data, f)
      benchmarks = perf_data_generator.get_scheduled_non_telemetry_benchmarks(
          fake_perf_waterfall_file)
      self.assertIn('ninja_test', benchmarks)
      self.assertIn('gun_slinger', benchmarks)
      self.assertIn('test_dancing', benchmarks)
      self.assertIn('test_singing', benchmarks)
    finally:
      os.remove(fake_perf_waterfall_file)


class TestIsPerfBenchmarksSchedulingValid(unittest.TestCase):
  def setUp(self):
    self.maxDiff = None
    self.original_GTEST_BENCHMARKS = copy.deepcopy(
        perf_data_generator.GTEST_BENCHMARKS)
    self.original_OTHER_BENCHMARKS = copy.deepcopy(
        perf_data_generator.OTHER_BENCHMARKS)
    self.test_stream = StringIO()
    self.mock_get_non_telemetry_benchmarks = mock.patch(
        'core.perf_data_generator.get_scheduled_non_telemetry_benchmarks')
    self.get_non_telemetry_benchmarks = (
        self.mock_get_non_telemetry_benchmarks.start())

  def tearDown(self):
    perf_data_generator.GTEST_BENCHMARKS = (
        self.original_GTEST_BENCHMARKS)
    perf_data_generator.OTHER_BENCHMARKS = (
        self.original_OTHER_BENCHMARKS)
    self.mock_get_non_telemetry_benchmarks.stop()

  def test_returnTrue(self):
    self.get_non_telemetry_benchmarks.return_value = {'honda'}

    perf_data_generator.GTEST_BENCHMARKS = {
        'honda': BenchmarkMetadata('baz@foo.com'),
    }
    perf_data_generator.OTHER_BENCHMARKS = {}
    valid = perf_data_generator.is_perf_benchmarks_scheduling_valid(
        'dummy', self.test_stream)

    self.assertEqual(self.test_stream.getvalue(), '')
    self.assertEqual(valid, True)

  def test_UnscheduledCppBenchmarks(self):
    self.get_non_telemetry_benchmarks.return_value = {'honda'}

    perf_data_generator.GTEST_BENCHMARKS = {
        'honda': BenchmarkMetadata('baz@foo.com'),
        'toyota': BenchmarkMetadata('baz@foo.com'),
    }
    perf_data_generator.OTHER_BENCHMARKS = {}
    valid = perf_data_generator.is_perf_benchmarks_scheduling_valid(
        'dummy', self.test_stream)

    self.assertEqual(valid, False)
    self.assertIn('Benchmark toyota is tracked but not scheduled',
        self.test_stream.getvalue())

  def test_UntrackedCppBenchmarks(self):
    self.get_non_telemetry_benchmarks.return_value = {'honda', 'tesla'}

    perf_data_generator.GTEST_BENCHMARKS = {
        'honda': BenchmarkMetadata('baz@foo.com'),
    }
    perf_data_generator.OTHER_BENCHMARKS = {}
    valid = perf_data_generator.is_perf_benchmarks_scheduling_valid(
        'dummy', self.test_stream)

    self.assertEqual(valid, False)
    self.assertIn(
        'Benchmark tesla is scheduled on perf waterfall but not tracked',
        self.test_stream.getvalue())
