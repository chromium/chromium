# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import unittest

from core import benchmark_finders
from telemetry import decorators


class TestGetBenchmarkNamesForFile(unittest.TestCase):

  def setUp(self):
    self.top_level_dir = os.path.abspath(
        os.path.join(os.path.dirname(__file__), 'test_data'))

  def testListSimpleBenchmarksDefinedInOneFile(self):
    self.assertEqual(
        benchmark_finders.GetBenchmarkNamesForFile(
            self.top_level_dir,
            os.path.join(self.top_level_dir, 'simple_benchmarks_case.py')), [
                'test_benchmark_1', 'test_benchmark_2',
                'test_benchmark_subclass_1', 'test_benchmark_subclass_2'
            ])

  @decorators.Disabled('all')  # http://crbug.com/637938
  def testListSimpleBenchmarksDefinedInOneFileComplex(self):
    self.assertEqual(
        benchmark_finders.GetBenchmarkNamesForFile(
            self.top_level_dir,
            os.path.join(self.top_level_dir, 'complex_benchmarks_case.py')), [
                'test_benchmark_complex_1', 'test_benchmark_complex_subclass',
                'test_benchmark_complex_subclass_from_other_module'
            ])
