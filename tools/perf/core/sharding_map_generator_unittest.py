# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import copy
import json
import os
import unittest

from core import sharding_map_generator


class TestShardingMapGenerator(unittest.TestCase):

  def _generate_test_data(self, times):
    timing_data = []
    benchmarks_data = []
    for i, _ in enumerate(times):
      b = {
        'name': 'benchmark_' + str(i),
        'stories': [],
        'repeat': 1,
      }
      benchmarks_data.append(b)
      story_times = times[i]
      for j, _ in enumerate(story_times):
        benchmark_name = 'benchmark_' + str(i)
        story_name = 'story_' + str(j)
        b['stories'].append(story_name)
        timing_data.append({
           'name': benchmark_name + '/' + story_name,
           'duration': story_times[j]
        })
    return benchmarks_data, timing_data

  def testGenerateAndTestShardingMap(self):
    benchmarks_data, timing_data, = self._generate_test_data(
        [[60, 56, 57], [66, 54, 80, 4], [2, 8, 7, 37, 2]])
    timing_data_for_testing = copy.deepcopy(timing_data)
    sharding_map = sharding_map_generator.generate_sharding_map(
        benchmarks_data, timing_data, 3, None)
    results = sharding_map_generator.test_sharding_map(sharding_map,
        benchmarks_data, timing_data_for_testing)
    self.assertEqual(results['0']['full_time'], 116)
    self.assertEqual(results['1']['full_time'], 177)
    self.assertEqual(results['2']['full_time'], 140)

  def testGenerateShardingMapsWithoutStoryTimingData(self):
    # Two tests benchmarks are to be sharded between 3 machines. The first one
    # has 4 stories, each repeat 2 times. The second one has 4 stories
    # without repeat. Without any assumption about timing, the best sharding
    # is to shard the first 2 stories of 'foo_benchmark' on shard 1, the next
    # two stories of 'foo_benchmark' on shard 2, and 'bar_benchmark' entirely on
    # shard 3.
    timing_data = []
    benchmarks_data = [
      { 'name': 'foo_benchmark',
        'stories': ['foo_1', 'foo_2', 'foo_3', 'foo_4'],
        'repeat': 2
      },
      { 'name': 'bar_benchmark',
        'stories': ['bar_1', 'bar_2', 'bar_3', 'bar_4'],
        'repeat': 1
      }

    ]
    sharding_map = sharding_map_generator.generate_sharding_map(
        benchmarks_data, timing_data, 3, None)

    self.assertEquals(
      sharding_map['0']['benchmarks'],
      collections.OrderedDict([('bar_benchmark', {'abridged': False})]))

    self.assertEquals(
      sharding_map['1']['benchmarks'],
      collections.OrderedDict([('foo_benchmark',
                                {'end': 2, 'abridged': False})]))

    self.assertEquals(
      sharding_map['2']['benchmarks'],
      collections.OrderedDict([('foo_benchmark',
                                {'begin': 2, 'abridged': False})]))

  def testGeneratePerfSharding(self):
    test_data_dir = os.path.join(os.path.dirname(__file__), 'test_data')
    with open(os.path.join(test_data_dir, 'benchmarks_to_shard.json')) as f:
      benchmarks_to_shard = json.load(f)

    with open(os.path.join(test_data_dir, 'test_timing_data.json')) as f:
      timing_data = json.load(f)

    with open(
        os.path.join(test_data_dir, 'test_timing_data_1_build.json')) as f:
      timing_data_single_build = json.load(f)

    sharding_map = sharding_map_generator.generate_sharding_map(
        benchmarks_to_shard, timing_data, num_shards=5, debug=False)

    results = sharding_map_generator.test_sharding_map(
        sharding_map, benchmarks_to_shard, timing_data_single_build)

    shards_timing = []
    for shard in results:
      shards_timing.append(results[shard]['full_time'])
    self.assertTrue(max(shards_timing) - min(shards_timing) < 300)
