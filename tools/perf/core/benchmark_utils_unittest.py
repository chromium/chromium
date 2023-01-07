# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest

from core import benchmark_utils

from telemetry import page
from telemetry import story
from telemetry import benchmark as benchmark_module

class ThousandAndOneStoriesBenchmark(benchmark_module.Benchmark):
  def CreateStorySet(self, options):
    del options  # unused
    story_set = story.StorySet()
    for i in range(1001):
      story_set.AddStory(
          page.Page(
              url='file://does-not-exist.html', name='story-number-%i' % i,
              page_set=story_set))
    return story_set


class BenchmarkUtilsUnittest(unittest.TestCase):
  def testGetBenchmarkStoryNamesOrdering(self):
    story_names = benchmark_utils.GetBenchmarkStoryNames(
        ThousandAndOneStoriesBenchmark())
    self.assertEqual(story_names, ['story-number-%i' % i for i in range(1001)])
