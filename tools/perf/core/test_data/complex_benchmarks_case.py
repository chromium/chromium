# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from core import perf_benchmark
from core.test_data import simple_benchmarks_case


class TestBenchmarkComplexFoo(perf_benchmark.PerfBenchmark):

  @classmethod
  def Name(cls):
    return 'test_benchmark_complex_1'


class TestBenchmarkComplexSubclass(TestBenchmarkComplexFoo):

  @classmethod
  def Name(cls):
    return 'test_benchmark_complex_subclass'


class TestBenchmarkComplexBar(simple_benchmarks_case.TestBenchmarkSubclassBar):

  @classmethod
  def Name(cls):
    return 'test_benchmark_complex_subclass_from_other_module'
