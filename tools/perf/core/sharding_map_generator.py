# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import collections
import sys

import core.path_util

core.path_util.AddTelemetryToPath()



def generate_sharding_map(
    benchmarks_to_shard, timing_data, num_shards, debug):
  """Generate sharding map.

    Args:
      benchmarks_to_shard is a list all benchmarks to be sharded. Its
      structure is as follows:
      [{
         "name": "benchmark_1",
         "stories": [ "storyA", "storyB",...],
         "repeat": <number of pageset_repeat>
        },
        {
         "name": "benchmark_2",
         "stories": [ "storyA", "storyB",...],
         "repeat": <number of pageset_repeat>
        },
         ...
      ]

      The "stories" field contains a list of ordered story names. Notes that
      this should match the actual order of how the benchmark stories are
      executed for the sharding algorithm to be effective.

  """
  # Sort the list of benchmarks to be sharded by benchmark's name to make the
  # execution of this algorithm deterministic.
  benchmarks_to_shard.sort(key=lambda entry: entry['name'])

  story_timing_list = _gather_timing_data(
      benchmarks_to_shard, timing_data, True)

  all_stories = {}
  for b in benchmarks_to_shard:
    all_stories[b['name']] = b['stories']

  total_time = sum(p[1] for p in story_timing_list)
  expected_time_per_shard = total_time/num_shards

  total_time_scheduled = 0
  sharding_map = collections.OrderedDict()
  debug_map = collections.OrderedDict()
  min_shard_time = sys.maxint
  min_shard_index = None
  max_shard_time = 0
  max_shard_index = None
  num_stories = len(story_timing_list)
  predicted_shard_timings = []

  # The algorithm below removes all the stories from |story_timing_list| one by
  # one and add them to the current shard until the shard's total time is
  # approximately equals to |expected_time_per_shard|. After that point,
  # it moves to the next shard.
  # For efficient removal of |story_timing_list|'s elements & to keep the
  # ordering of benchmark alphabetically sorted in the shards' assignment, we
  # reverse the |story_timing_list|.
  story_timing_list.reverse()
  num_stories = len(story_timing_list)
  final_shard_index = num_shards - 1
  for i in range(num_shards):
    shard_name = 'shard #%i' % i
    sharding_map[str(i)] = {'benchmarks': collections.OrderedDict()}
    debug_map[shard_name] = collections.OrderedDict()
    time_per_shard = 0
    stories_in_shard = []
    expected_total_time = expected_time_per_shard * (i + 1)
    last_diff = abs(total_time_scheduled - expected_total_time)
    # Keep adding story to the current shard until either:
    # * The absolute difference between the total time of shards so far and
    #   expected total time is minimal.
    # * The shard is final shard, and there is no more stories to add.
    #
    # Note: we do check for the final shard in case due to rounding error,
    # the last_diff can be minimal even if we don't add all the stories to the
    # final shard.
    while story_timing_list:
      candidate_story, candidate_story_duration = story_timing_list[-1]
      new_diff = abs(total_time_scheduled + candidate_story_duration -
                     expected_total_time)
      if new_diff < last_diff or i == final_shard_index:
        story_timing_list.pop()
        total_time_scheduled += candidate_story_duration
        time_per_shard += candidate_story_duration
        stories_in_shard.append(candidate_story)
        debug_map[shard_name][candidate_story] = candidate_story_duration
        last_diff = abs(total_time_scheduled - expected_total_time)
        _add_benchmarks_to_shard(sharding_map, i, stories_in_shard, all_stories)
      else:
        break
    # Double time_per_shard to account for reference benchmark run.
    debug_map[shard_name]['expected_total_time'] = time_per_shard * 2
    if time_per_shard > max_shard_time:
      max_shard_time = time_per_shard
      max_shard_index = i
    if time_per_shard < min_shard_time:
      min_shard_time = time_per_shard
      min_shard_index = i

    predicted_shard_timings.append((shard_name, time_per_shard * 2))

  sharding_map['extra_infos'] = collections.OrderedDict([
      ('num_stories', num_stories),
      # Double all the time stats by 2 to account for reference build.
      ('predicted_min_shard_time', min_shard_time * 2),
      ('predicted_min_shard_index', min_shard_index),
      ('predicted_max_shard_time', max_shard_time * 2),
      ('predicted_max_shard_index', max_shard_index),
      ])

  if debug:
    sharding_map['extra_infos'].update(debug_map)
  else:
    sharding_map['extra_infos'].update(predicted_shard_timings)
  return sharding_map


def _add_benchmarks_to_shard(sharding_map, shard_index, stories_in_shard,
    all_stories):
  benchmarks = collections.OrderedDict()
  for story in stories_in_shard:
    (b, story) = story.split('/', 1)
    if b not in benchmarks:
      benchmarks[b] = []
    benchmarks[b].append(story)

  # Format the benchmark's stories by indices
  benchmarks_in_shard = collections.OrderedDict()
  for b in benchmarks:
    benchmarks_in_shard[b] = {}
    first_story = all_stories[b].index(benchmarks[b][0])
    last_story = all_stories[b].index(benchmarks[b][-1]) + 1
    if first_story != 0:
      benchmarks_in_shard[b]['begin'] = first_story
    if last_story != len(all_stories[b]):
      benchmarks_in_shard[b]['end'] = last_story
  sharding_map[str(shard_index)] = {'benchmarks': benchmarks_in_shard}


def _gather_timing_data(benchmarks_to_shard, timing_data, repeat):
  story_timing_dict = {}
  benchmarks_data_by_name = {}
  for b in benchmarks_to_shard:
    story_list = b['stories']
    benchmarks_data_by_name[b['name']] = b
    # Initialize the duration of all stories to be shard to 1 * repeat.
    # The reasons are:
    # 1) Even if the stories are skipped, they still have non neligible
    #    overhead.
    # 2) For a case of sharding a set of benchmarks with no existing data about
    #    timing, initializing the stories time within a single repeat to 1 leads
    #    to a roughly equal distribution of stories on the shards, whereas
    #    initializing them to zero will make the algorithm put all the stories
    #    into the first shard.
    for story in story_list:
      story_timing_dict[b['name'] + '/' + story] = b['repeat']

  for run in timing_data:
    benchmark = run['name'].split('/', 1)[0]
    if run['name'] in story_timing_dict:
      if run['duration']:
        if repeat:
          story_timing_dict[run['name']] = (float(run['duration'])
              * benchmarks_data_by_name[benchmark]['repeat'])
        else:
          story_timing_dict[run['name']] = float(run['duration'])
  story_timing_list = []
  for entry in benchmarks_to_shard:
    benchmark_name = entry['name']
    for story_name in entry['stories']:
      test_name = '%s/%s' % (benchmark_name, story_name)
      story_timing_list.append((test_name, story_timing_dict[test_name]))
  return story_timing_list


def _generate_empty_sharding_map(num_shards):
  sharding_map = collections.OrderedDict()
  for i in range(0, num_shards):
    sharding_map[str(i)] = {'benchmarks': collections.OrderedDict()}
  return sharding_map


def test_sharding_map(
    sharding_map, benchmarks_to_shard, test_timing_data):
  story_timing_list = _gather_timing_data(
      benchmarks_to_shard, test_timing_data, False)

  story_timing_dict = dict(story_timing_list)

  results = collections.OrderedDict()
  all_stories = {}
  for b in benchmarks_to_shard:
    all_stories[b['name']] = b['stories']

  sharding_map.pop('extra_infos', None)
  for shard in sharding_map:
    results[shard] = collections.OrderedDict()
    shard_total_time = 0
    for benchmark_name in sharding_map[shard]['benchmarks']:
      benchmark = sharding_map[shard]['benchmarks'][benchmark_name]
      begin = 0
      end = len(all_stories[benchmark_name])
      if 'begin' in benchmark:
        begin = benchmark['begin']
      if 'end' in benchmark:
        end = benchmark['end']
      benchmark_timing = 0
      for story in all_stories[benchmark_name][begin : end]:
        story_timing = story_timing_dict[benchmark_name + '/' + story]
        results[shard][benchmark_name + '/' + story] = str(story_timing)
        benchmark_timing += story_timing
      shard_total_time += benchmark_timing
    results[shard]['full_time'] = shard_total_time
  return results
