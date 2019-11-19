# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import copy
import cStringIO
import unittest
import tempfile
import json
import os

import mock

from core import perf_data_generator
from core.perf_data_generator import BenchmarkMetadata


class PerfDataGeneratorTest(unittest.TestCase):
  def setUp(self):
    # Test config can be big, so set maxDiff to None to see the full comparision
    # diff when assertEquals fails.
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
      self.assertEquals(
          perf_data_generator.get_scheduled_non_telemetry_benchmarks(
              fake_perf_waterfall_file),
          {'ninja_test', 'gun_slinger', 'test_dancing', 'test_singing'})
    finally:
      os.remove(fake_perf_waterfall_file)


class TestIsPerfBenchmarksSchedulingValid(unittest.TestCase):
  def setUp(self):
    self.maxDiff = None
    self.original_GTEST_BENCHMARKS = copy.deepcopy(
        perf_data_generator.GTEST_BENCHMARKS)
    self.original_OTHER_BENCHMARKS = copy.deepcopy(
        perf_data_generator.OTHER_BENCHMARKS)
    self.test_stream = cStringIO.StringIO()
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

    self.assertEquals(valid, True)
    self.assertEquals(self.test_stream.getvalue(), '')

  def test_UnscheduledCppBenchmarks(self):
    self.get_non_telemetry_benchmarks.return_value = {'honda'}

    perf_data_generator.GTEST_BENCHMARKS = {
        'honda': BenchmarkMetadata('baz@foo.com'),
        'toyota': BenchmarkMetadata('baz@foo.com'),
    }
    perf_data_generator.OTHER_BENCHMARKS = {}
    valid = perf_data_generator.is_perf_benchmarks_scheduling_valid(
        'dummy', self.test_stream)

    self.assertEquals(valid, False)
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

    self.assertEquals(valid, False)
    self.assertIn(
        'Benchmark tesla is scheduled on perf waterfall but not tracked',
        self.test_stream.getvalue())
