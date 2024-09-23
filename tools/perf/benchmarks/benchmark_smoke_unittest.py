# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run the first page of one benchmark for every module.

Only benchmarks that have a composable measurement are included.
Ideally this test would be comprehensive, however, running one page
of every benchmark would run impractically long.
"""

import os
import sys
import unittest

from chrome_telemetry_build import chromium_config

from core import results_processor
from core import testing

from telemetry import benchmark as benchmark_module
from telemetry import decorators
from telemetry.testing import progress_reporter

from py_utils import discover
from py_utils import tempfile_ext

from benchmarks import jetstream2
from benchmarks import octane
from benchmarks import rasterize_and_record_micro
from benchmarks import speedometer1
from benchmarks import v8_browsing


# We want to prevent benchmarks from accidentally trying to upload too much
# data to the chrome perf dashboard. So the smoke tests below cap the max
# number of values that each story tested would produce when running on the
# waterfall.
MAX_VALUES_PER_TEST_CASE = 1000


def SmokeTestGenerator(benchmark_class, num_pages=1, story_tag_filter=None):
  """Generates a smoke test for the first N pages from a benchmark.

  Args:
    benchmark_class: a benchmark class to smoke test.
    num_pages: only smoke test the first N pages, since smoke testing
      everything would take too long to run.
    story_tag_filter: only smoke test stories matching with tags.
  """
  # NOTE TO SHERIFFS: DO NOT DISABLE THIS TEST.
  #
  # This smoke test dynamically tests all benchmarks. So disabling it for one
  # failing or flaky benchmark would disable a much wider swath of coverage
  # than is usually intended. Instead, if a particular benchmark is failing,
  # disable it in tools/perf/benchmarks/*.
  @decorators.Disabled('android')  # crbug.com/641934
  def BenchmarkSmokeTestFunc(self):
    # Some benchmarks are running multiple iterations
    # which is not needed for a smoke test
    if hasattr(benchmark_class, 'enable_smoke_test_mode'):
      benchmark_class.enable_smoke_test_mode = True

    with tempfile_ext.NamedTemporaryDirectory() as temp_dir:
      options = testing.GetRunOptions(
          output_dir=temp_dir,
          benchmark_cls=benchmark_class,
          overrides={
              'story_shard_end_index': num_pages,
              'story_tag_filter': story_tag_filter
          },
          environment=chromium_config.GetDefaultChromiumConfig())
      options.pageset_repeat = 1  # For smoke testing only run the page once.
      options.output_formats = ['histograms']
      options.max_values_per_test_case = MAX_VALUES_PER_TEST_CASE
      results_processor.ProcessOptions(options)

      return_code = benchmark_class().Run(options)
      # TODO(crbug.com/40105219): Make 111 be the exit code that means
      # "no stories were run.".
      if return_code in (-1, 111):
        self.skipTest('The benchmark was not run.')
      self.assertEqual(
          return_code, 0,
          msg='Benchmark run failed: %s' % benchmark_class.Name())
      return_code = results_processor.ProcessResults(options, is_unittest=True)
      self.assertEqual(
          return_code, 0,
          msg='Result processing failed: %s' % benchmark_class.Name())

  # Set real_test_func as benchmark_class to make typ
  # write benchmark_class source filepath to trace instead of
  # path to this file
  BenchmarkSmokeTestFunc.real_test_func = benchmark_class

  return BenchmarkSmokeTestFunc


# The list of benchmark modules to be excluded from our smoke tests.
_BLOCK_LIST_TEST_MODULES = {
    octane,  # Often fails & take long time to timeout on cq bot.
    rasterize_and_record_micro,  # Always fails on cq bot.
    speedometer1,  # Takes 101 seconds.
    jetstream2,  # Causes CQ shard to timeout, crbug.com/992837
    v8_browsing,  # Flaky on Android, crbug.com/628368.
}

# The list of benchmark names to be excluded from our smoke tests.
_BLOCK_LIST_TEST_NAMES = [
    'memory.long_running_idle_gmail_background_tbmv2',
    'UNSCHEDULED_ad_frames.iframe',  # b/342449133
    'UNSCHEDULED_oortonline_tbmv2',
    'webrtc',  # crbug.com/932036
    'v8.runtime_stats.top_25',  # Fails in Windows, crbug.com/1043048
    'wasmpspdfkit',  # Fails in Chrome OS, crbug.com/1191938
    'memory.desktop',  # crbug.com/1277277 and b/286898261
    'desktop_ui' if sys.platform == 'darwin' else None,  # crbug.com/1370958
    'power.desktop' if sys.platform == 'darwin' else None,  # crbug.com/1370958
]


def MergeDecorators(method, method_attribute, benchmark, benchmark_attribute):
  # Do set union of attributes to eliminate duplicates.
  merged_attributes = getattr(method, method_attribute, set()).union(
      getattr(benchmark, benchmark_attribute, set()))
  if merged_attributes:
    setattr(method, method_attribute, merged_attributes)


class BenchmarkSmokeTest(unittest.TestCase):
  pass


def load_tests(loader, standard_tests, pattern):
  del loader, standard_tests, pattern  # unused
  suite = progress_reporter.TestSuite()

  benchmarks_dir = os.path.dirname(__file__)
  top_level_dir = os.path.dirname(benchmarks_dir)

  # Using the default of |index_by_class_name=False| means that if a module
  # has multiple benchmarks, only the last one is returned.
  all_benchmarks = discover.DiscoverClasses(
      benchmarks_dir, top_level_dir, benchmark_module.Benchmark,
      index_by_class_name=False).values()
  for benchmark in all_benchmarks:
    if sys.modules[benchmark.__module__] in _BLOCK_LIST_TEST_MODULES:
      continue
    if benchmark.Name() in _BLOCK_LIST_TEST_NAMES:
      continue

    if 'desktop_ui' in benchmark.Name():
      # Run tests with a specific smoke_test tag.
      method = SmokeTestGenerator(benchmark,
                                  num_pages=None,
                                  story_tag_filter='smoke_test')
    else:
      method = SmokeTestGenerator(benchmark)

    # Make sure any decorators are propagated from the original declaration.
    # (access to protected members) pylint: disable=protected-access
    # TODO(dpranke): Since we only pick the first test from every class
    # (above), if that test is disabled, we'll end up not running *any*
    # test from the class. We should probably discover all of the tests
    # in a class, and then throw the ones we don't need away instead.

    disabled_benchmark_attr = decorators.DisabledAttributeName(benchmark)
    disabled_method_attr = decorators.DisabledAttributeName(method)
    enabled_benchmark_attr = decorators.EnabledAttributeName(benchmark)
    enabled_method_attr = decorators.EnabledAttributeName(method)

    MergeDecorators(method, disabled_method_attr, benchmark,
                    disabled_benchmark_attr)
    MergeDecorators(method, enabled_method_attr, benchmark,
                    enabled_benchmark_attr)

    setattr(BenchmarkSmokeTest, benchmark.Name(), method)

    suite.addTest(BenchmarkSmokeTest(benchmark.Name()))

  return suite
