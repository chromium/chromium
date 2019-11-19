# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""For all the benchmarks that set options, test that the options are valid."""

from collections import defaultdict
import unittest

from core import path_util
from core import perf_benchmark

from telemetry import benchmark as benchmark_module
from telemetry import decorators
from telemetry.testing import options_for_unittests
from telemetry.testing import progress_reporter

from py_utils import discover

def _GetAllPerfBenchmarks():
  return discover.DiscoverClasses(
      path_util.GetOfficialBenchmarksDir(), path_util.GetPerfDir(),
      benchmark_module.Benchmark, index_by_class_name=True).values()


def _BenchmarkOptionsTestGenerator(benchmark):
  def testBenchmarkOptions(self):
    """Tests whether benchmark options can be constructed without errors."""
    try:
      options_for_unittests.GetRunOptions(benchmark_cls=benchmark)
    except benchmark_module.InvalidOptionsError as exc:
      self.fail(str(exc))
  return testBenchmarkOptions


class TestNoBenchmarkNamesDuplication(unittest.TestCase):

  def runTest(self):
    all_benchmarks = _GetAllPerfBenchmarks()
    names_to_benchmarks = defaultdict(list)
    for b in all_benchmarks:
      names_to_benchmarks[b.Name()].append(b)
    for n in names_to_benchmarks:
      self.assertEquals(1, len(names_to_benchmarks[n]),
                        'Multiple benchmarks with the same name %s are '
                        'found: %s' % (n, str(names_to_benchmarks[n])))


class TestBenchmarkNamingMobile(unittest.TestCase):

  # TODO(rnephew): This needs to be fixed after we move to CanRunOnBrowser.
  @decorators.Disabled('all')
  def runTest(self):
    all_benchmarks = _GetAllPerfBenchmarks()
    names_to_benchmarks = defaultdict(list)
    for b in all_benchmarks:
      names_to_benchmarks[b.Name()] = b

    for n, bench in names_to_benchmarks.items():
      if 'mobile' in n:
        enabled_tags = decorators.GetEnabledAttributes(bench)
        disabled_tags = decorators.GetDisabledAttributes(bench)

        self.assertTrue('all' in disabled_tags or 'android' in enabled_tags,
                        ','.join([
                            str(bench), bench.Name(),
                            str(disabled_tags), str(enabled_tags)]))


class TestNoOverrideCustomizeOptions(unittest.TestCase):

  def runTest(self):
    all_benchmarks = _GetAllPerfBenchmarks()
    for benchmark in all_benchmarks:
      self.assertEquals(True, issubclass(benchmark,
                                         perf_benchmark.PerfBenchmark),
                        'Benchmark %s needs to subclass from PerfBenchmark'
                        % benchmark.Name())
      self.assertEquals(
          benchmark.CustomizeOptions,
          perf_benchmark.PerfBenchmark.CustomizeOptions,
          'Benchmark %s should not override CustomizeOptions' %
          benchmark.Name())


def _AddBenchmarkOptionsTests(suite):
  # Using |index_by_class_name=True| allows returning multiple benchmarks
  # from a module.
  all_benchmarks = _GetAllPerfBenchmarks()
  for benchmark in all_benchmarks:
    if not benchmark.options:
      # No need to test benchmarks that have not defined options.
      continue

    class BenchmarkOptionsTest(unittest.TestCase):
      pass
    setattr(BenchmarkOptionsTest, benchmark.Name(),
            _BenchmarkOptionsTestGenerator(benchmark))
    suite.addTest(BenchmarkOptionsTest(benchmark.Name()))


def load_tests(loader, standard_tests, pattern):
  del loader, pattern  # unused
  suite = progress_reporter.TestSuite()
  for t in standard_tests:
    suite.addTests(t)
  _AddBenchmarkOptionsTests(suite)
  return suite
