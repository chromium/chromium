# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import collections
import copy
import json
import os
from typing import Iterable, Optional
import unittest
from pathlib import Path
import sys

# Add tools/perf to sys.path
FILE_PATH = Path(__file__).resolve()
sys.path.append(str(FILE_PATH.parents[1]))

from core import path_util

path_util.AddTelemetryToPath()

from core import bot_platforms
from core import sharding_map_generator


class FakeBenchmark:

  def __init__(self, name):
    self._name = name

  def Name(self):
    return self._name


class FakeBenchmarkConfig(bot_platforms.TelemetryConfig):

  def __init__(self,
               name: str,
               stories: Iterable[str],
               repeat: Optional[int] = None):
    super().__init__(FakeBenchmark(name), repeat=repeat, abridged=False)
    self._stories: tuple[str, ...] = tuple(stories)

  @property
  def stories(self) -> tuple[str, ...]:
    return self._stories


class TestShardingMapGenerator(unittest.TestCase):

  def _generate_test_data(self, times):
    timing_data = []
    benchmarks_data = []
    for i, story_times in enumerate(times):
      benchmark_name = 'benchmark_' + str(i)
      stories = []
      for j, duration in enumerate(story_times):
        story_name = 'story_' + str(j)
        stories.append(story_name)
        timing_data.append({
            'name': benchmark_name + '/' + story_name,
            'duration': duration
        })
      b = FakeBenchmarkConfig(benchmark_name, stories, 1)
      benchmarks_data.append(b)
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
    # 3 benchmarks are to be sharded between 3 machines. The first one
    # has 4 stories, each repeat 2 times. The second one has 4 stories
    # without repeat. Without any assumption about timing, the best sharding
    # is to put each benchmark on its own device. Repeats do not necessarily
    # imply that a story will take longer than another story that is not
    # repeated. This is because short stories tend to be repeated, whereas long
    # stories tend to not be repeated.
    timing_data = []
    benchmarks_data = [
        FakeBenchmarkConfig('a_benchmark',
                            stories=['a_1', 'a_2', 'a_3', 'a_4'],
                            repeat=2),
        FakeBenchmarkConfig('b_benchmark',
                            stories=['b_1', 'b_2', 'b_3', 'b_4'],
                            repeat=1),
        FakeBenchmarkConfig('c_benchmark',
                            stories=['c_1', 'c_2', 'c_3', 'c_4'],
                            repeat=1),
    ]
    sharding_map = sharding_map_generator.generate_sharding_map(
        benchmarks_data, timing_data, 3, None)
    self.assertEqual(
        sharding_map['0']['benchmarks'],
        collections.OrderedDict([('a_benchmark', {
            'abridged': False,
            'pageset_repeat': 2,
        })]))
    self.assertEqual(
        sharding_map['1']['benchmarks'],
        collections.OrderedDict([('b_benchmark', {
            'abridged': False
        })]))
    self.assertEqual(
        sharding_map['2']['benchmarks'],
        collections.OrderedDict([('c_benchmark', {
            'abridged': False
        })]))

  def testGenerateShardingMapsWithPagesetRepeatOverride(self):
    timing_data = []
    benchmarks_data = [
        FakeBenchmarkConfig('a_benchmark',
                            stories=['a_1', 'a_2', 'a_3', 'a_4'],
                            repeat=2),
        FakeBenchmarkConfig('b_benchmark',
                            stories=['b_1', 'b_2', 'b_3', 'b_4'],
                            repeat=1),
    ]
    sharding_map = sharding_map_generator.generate_sharding_map(
        benchmarks_data, timing_data, 1, None)
    self.assertEqual(
        sharding_map['0']['benchmarks'],
        collections.OrderedDict([('a_benchmark', {
            'abridged': False,
            'pageset_repeat': 2,
        }), ('b_benchmark', {
            'abridged': False,
        })]))

  def testGeneratePerfSharding(self):
    test_data_dir = os.path.join(os.path.dirname(__file__), 'test_data')
    with open(os.path.join(test_data_dir, 'benchmarks_to_shard.json')) as f:
      raw_configs = json.load(f)
      benchmarks_to_shard = [FakeBenchmarkConfig(
          c['name'], c['stories'], c['repeat']) for c in raw_configs]

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
    self.assertTrue(max(shards_timing) - min(shards_timing) < 600)

  def testGenerateAndTestShardingMapWithCrossDeviceTargetCap(self):
    benchmarks_data, timing_data, = self._generate_test_data(
        [[10, 20, 30], [45, 35, 25, 15], [50, 40, 30, 42, 10]])
    target_devices = {'benchmark_2': {'story_0': 5}}
    sharding_map = sharding_map_generator.generate_sharding_map(
        benchmarks_data, timing_data, 3, None, target_devices)
    self.assertIn('benchmark_2', sharding_map['0']['benchmarks'])
    self.assertIn('benchmark_2', sharding_map['1']['benchmarks'])
    self.assertIn('benchmark_2', sharding_map['2']['benchmarks'])

  def testGenerateAndTestShardingMapWithCrossDevice(self):
    benchmarks_data, timing_data, = self._generate_test_data(
        [[10, 20, 30], [45, 35, 25, 15], [50, 40, 30, 20, 10]])
    repeat_config = {
        'benchmark_0': {
            'story_1': 2
        },
        'benchmark_1': {
            'story_2': 3
        }
    }
    sharding_map = sharding_map_generator.generate_sharding_map(
        benchmarks_data, timing_data, 3, None, repeat_config)
    self.assertIn('benchmark_0', sharding_map['0']['benchmarks'])
    self.assertIn('benchmark_0', sharding_map['1']['benchmarks'])
    self.assertNotIn('benchmark_0', sharding_map['2']['benchmarks'])
    self.assertIn('benchmark_1', sharding_map['0']['benchmarks'])
    self.assertIn('benchmark_1', sharding_map['1']['benchmarks'])
    self.assertIn('benchmark_1', sharding_map['2']['benchmarks'])

  def testGenerateAndTestShardingMapWithBenchmarkRepeats(self):
    benchmarks_data, timing_data, = self._generate_test_data(
        [[10, 20, 30], [45, 35, 25, 15], [50, 40, 30, 20, 10]])
    repeat_config = {'benchmark_0': 2, 'benchmark_1': {'story_2': 3}}
    sharding_map = sharding_map_generator.generate_sharding_map(
        benchmarks_data, timing_data, 3, None, repeat_config)
    self.assertIn('benchmark_0', sharding_map['0']['benchmarks'])
    # only the 'abridged' key when the whole benchmark is on this shard
    self.assertEqual(1, len(sharding_map['0']['benchmarks']['benchmark_0']))
    self.assertIn('benchmark_0', sharding_map['1']['benchmarks'])
    self.assertEqual(1, len(sharding_map['1']['benchmarks']['benchmark_0']))
    self.assertNotIn('benchmark_0', sharding_map['2']['benchmarks'])
    self.assertIn('benchmark_1', sharding_map['0']['benchmarks'])
    self.assertIn('benchmark_1', sharding_map['1']['benchmarks'])
    self.assertIn('benchmark_1', sharding_map['2']['benchmarks'])

  def testGenerateAndTestShardingMapWithBenchmarkRepeatsCrossShards(self):
    benchmarks_data, timing_data, = self._generate_test_data(
        [[10, 20, 30], [65, 55, 5, 45], [50, 40, 30, 20, 10]])
    repeat_config = {'benchmark_1': {'story_2': 10}, 'benchmark_2': 2}
    sharding_map = sharding_map_generator.generate_sharding_map(
        benchmarks_data, timing_data, 5, None, repeat_config)
    # benchmark_2 takes two shards, and thus will be in 4 shards
    self.assertIn('benchmark_2', sharding_map['0']['benchmarks'])
    self.assertIn('benchmark_2', sharding_map['1']['benchmarks'])
    self.assertIn('benchmark_2', sharding_map['2']['benchmarks'])
    self.assertIn('benchmark_2', sharding_map['3']['benchmarks'])
    self.assertNotIn('benchmark_2', sharding_map['4']['benchmarks'])
    self.assertNotIn('benchmark_0', sharding_map['0']['benchmarks'])
    self.assertIn('benchmark_0', sharding_map['1']['benchmarks'])
    self.assertIn('benchmark_1', sharding_map['0']['benchmarks'])
    self.assertIn('benchmark_1', sharding_map['1']['benchmarks'])
    self.assertIn('benchmark_1', sharding_map['2']['benchmarks'])
    self.assertIn('benchmark_1', sharding_map['3']['benchmarks'])
    self.assertIn('benchmark_1', sharding_map['4']['benchmarks'])

  def testGenerateShardingMapWithCrossbench(self):
    benchmarks_data, timing_data, = self._generate_test_data(
        [[10, 20, 30], [65, 55, 5, 45], [50, 40, 30, 20, 10]])
    benchmarks_data.append(
        bot_platforms.CrossbenchConfig('cb_benchmark_0',
                                       'cb_benchmark_0_name',
                                       flags=['--my_arg']))
    sharding_map = sharding_map_generator.generate_sharding_map(
        benchmarks_data, timing_data, 3, None)
    self.assertIn('crossbench', sharding_map['2'])
    self.assertIn('cb_benchmark_0', sharding_map['2']['crossbench'])
    self.assertEqual(
        'cb_benchmark_0_name',
        sharding_map['2']['crossbench']['cb_benchmark_0']['crossbench_name'])
    self.assertEqual(
        ['--my_arg', '--enable-field-trials'],
        sharding_map['2']['crossbench']['cb_benchmark_0']['arguments'])


if __name__ == '__main__':
  unittest.main()
