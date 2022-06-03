# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Dummy benchmarks for the bisect FYI integration tests and other tests.

The number they produce aren't meant to represent any actual performance
data of the browser. For more information about these dummy benchmarks,
see: https://goo.gl/WvZiiW
"""

import random

from telemetry import benchmark
from telemetry.page import legacy_page_test

from core import perf_benchmark
from page_sets import dummy_story_set


class _DummyTest(legacy_page_test.LegacyPageTest):

  def __init__(self, avg, std):
    super(_DummyTest, self).__init__()
    self._avg = avg
    self._std = std

  def ValidateAndMeasurePage(self, page, tab, results):
    del tab  # unused
    value = random.gauss(self._avg, self._std)
    results.AddMeasurement('gaussian-value', 'ms', value)


class _DummyBenchmark(perf_benchmark.PerfBenchmark):
  page_set = dummy_story_set.DummyStorySet


@benchmark.Info(
    emails=['johnchen@chromium.org', 'wenbinzhang@google.com'],
    component='Test>Telemetry')
class DummyBenchmarkOne(_DummyBenchmark):
  """A low noise benchmark with mean=100 & std=1."""

  def CreatePageTest(self, options):
    return _DummyTest(168, 1)

  @classmethod
  def Name(cls):
    return 'dummy_benchmark.stable_benchmark_1'


@benchmark.Info(
    emails=['johnchen@chromium.org', 'wenbinzhang@google.com'],
    component='Test>Telemetry')
class DummyBenchmarkTwo(_DummyBenchmark):
  """A noisy benchmark with mean=50 & std=20."""

  def CreatePageTest(self, options):
    return _DummyTest(50, 20)

  @classmethod
  def Name(cls):
    return 'dummy_benchmark.noisy_benchmark_1'
