# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Base class for a PressBenchmark.

This benchmark manages both PressStory objects that
implement javascript based metrics as well as can
compute TMBv2 metrics.

Example implementation:

  FooPressBenchmark(press._PressBenchmark):
    @classmethod
    def Name(clas):
      return Foo;

    def CreateStorySet():
      // Return a set of stories inheriting from
      // page_sets.PressStory

    def CreateCoreTimelineBasedMeasurementOptions()
      // Implement to define tracing metrics you
      // want on top of any javascript metrics
      // implemented in your stories
"""
from core import perf_benchmark

from measurements import dual_metric_measurement

class _PressBenchmark(perf_benchmark.PerfBenchmark):
  test = dual_metric_measurement.DualMetricMeasurement
