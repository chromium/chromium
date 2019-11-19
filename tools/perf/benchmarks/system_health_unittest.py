# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import unittest

from benchmarks import system_health as system_health_benchmark
from core import path_util
from page_sets.system_health import system_health_stories
from page_sets.system_health import system_health_story

from telemetry import benchmark as benchmark_module

from py_utils import discover


def _GetAllSystemHealthBenchmarks():
  all_perf_benchmarks = discover.DiscoverClasses(
      path_util.GetOfficialBenchmarksDir(), path_util.GetPerfDir(),
      benchmark_module.Benchmark,
      index_by_class_name=True).values()
  return [b for b in all_perf_benchmarks if
          sys.modules[b.__module__] == system_health_benchmark]


class TestSystemHealthBenchmarks(unittest.TestCase):

  def testNamePrefix(self):
    for b in _GetAllSystemHealthBenchmarks():
      self.assertTrue(
          b.Name().startswith('system_health.'),
          '%r must have name starting with "system_health." prefix' % b)

  def testSystemHealthStorySetIsUsed(self):
    for b in _GetAllSystemHealthBenchmarks():
      if b is system_health_benchmark.WebviewStartupSystemHealthBenchmark:
        continue
      self.assertIsInstance(
          b().CreateStorySet(None),
          system_health_stories.SystemHealthStorySet,
          '%r does not use SystemHealthStorySet' % b)


class TestSystemHealthStories(unittest.TestCase):

  def testNoOverrideRunPageInteractions(self):
    desktop_stories = (
        system_health_stories.DesktopSystemHealthStorySet().stories)
    mobile_stories = (
        system_health_stories.MobileSystemHealthStorySet().stories)
    for s in desktop_stories + mobile_stories:
      # Long running stories has their own way of collecting memory dumps,
      # so they explicitly override RunPageInteractions method.
      if s.name.startswith('long_running:'):
        continue
      self.assertEquals(s.__class__.RunPageInteractions,
          system_health_story.SystemHealthStory.RunPageInteractions,
          'Story %s overrides RunPageInteractions. Override _DidLoadDocument '
          'instead' % s.name)
