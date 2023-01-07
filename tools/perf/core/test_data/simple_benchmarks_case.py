# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark


class _TestBenchmarkFoo(perf_benchmark.PerfBenchmark):

  @classmethod
  def Name(cls):
    return 'test_benchmark_1'


class TestBenchmarkBar(perf_benchmark.PerfBenchmark):

  @classmethod
  def Name(cls):
    return 'test_benchmark_2'


class TestBenchmarkSubclassBar(_TestBenchmarkFoo):

  @classmethod
  def Name(cls):
    return 'test_benchmark_subclass_1'


class TestBenchmarkSubclassFoo(TestBenchmarkBar):

  @classmethod
  def Name(cls):
    return 'test_benchmark_subclass_2'
