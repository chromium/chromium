# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

from telemetry import benchmark as benchmark_module
from telemetry.page import legacy_page_test
from telemetry.testing import options_for_unittests
from telemetry.web_perf import timeline_based_measurement

from py_utils import discover


def _GetAllPossiblePageTestInstances():
  page_test_instances = []
  measurements_dir = os.path.dirname(__file__)
  top_level_dir = os.path.dirname(measurements_dir)
  benchmarks_dir = os.path.join(top_level_dir, 'benchmarks')

  # Get all page test instances from measurement classes that are directly
  # constructible
  all_measurement_classes = discover.DiscoverClasses(
      measurements_dir, top_level_dir, legacy_page_test.LegacyPageTest,
      index_by_class_name=True, directly_constructable=True).values()
  for measurement_class in all_measurement_classes:
    page_test_instances.append(measurement_class())

  all_benchmarks_classes = discover.DiscoverClasses(
      benchmarks_dir, top_level_dir, benchmark_module.Benchmark).values()

  # Get all page test instances from defined benchmarks.
  # Note: since this depends on the command line options, there is no guarantee
  # that this will generate all possible page test instances but it's worth
  # enough for smoke test purpose.
  for benchmark_cls in all_benchmarks_classes:
    options = options_for_unittests.GetRunOptions(benchmark_cls=benchmark_cls)
    pt = benchmark_cls().CreatePageTest(options)
    if not isinstance(pt, timeline_based_measurement.TimelineBasedMeasurement):
      page_test_instances.append(pt)

  return page_test_instances


class MeasurementSmokeTest(unittest.TestCase):
  # Simple smoke test to make sure that all page_test are constructible.

  def testAllMeasurementInstance(self):
    _GetAllPossiblePageTestInstances()
