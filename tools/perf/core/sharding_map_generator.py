# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import collections

import core.bot_platforms
import core.path_util
import core.cli_utils

core.path_util.AddTelemetryToPath()

# Initialize the duration of all stories to be shard to 10 seconds.
# The reasons are:
# 1) Even if the stories are skipped, they still have non negligible
#    overhead.
# 2) For a case of sharding a set of benchmarks with no existing data about
#    timing, initializing the stories time within a single repeat to 1 leads
#    to a roughly equal distribution of stories on the shards, whereas
#    initializing them to zero will make the algorithm put all the stories
#    into the first shard.
# 3) For the case  of adding a new benchmark to a builder that hasn't run
#    it before but has run other benchmarks, 10 seconds is a reasonable
#    amount of time to guess that it would take the stories to run and
#    creates reasonably balanced shard maps.
DEFAULT_STORY_DURATION = 10


def generate_sharding_map(benchmarks_to_shard,
                          timing_data,
                          num_shards,
                          debug,
                          repeat_config=None):
  """Generate sharding map.

    Args:
      benchmarks_to_shard: a list of bot_platforms.BenchmarkConfig and
      ExecutableConfig objects.
      timing_data: The timing data in json with 'name' and 'duration'
      num_shards: the total number of shards
      debug: if true, print out full list of stories of each shard in shard map.
      repeat_config: dict of the tests which need to repeat on multiple
      shards.
    Return:
      The shard map.
  """
  # Now we have three ways to run the tests:
  # - Run the benchmark once;
  # - Run the benchmark multiple times;
  # - Run the test multiple times.
  # The current format of the shard map (which is using the benchmark and story
  # name as the key) does not directly support running repeating the same test
  # on the same shard. Also considering reusing the existing code, now we will
  # allocate the test in the following fashion: the benchmarks or tests which
  # needs repeats are always allocated first, in order to avoid the fact that
  # all of them need to be squeezed together.

  # Sort the list of benchmarks to be sharded by benchmark's name to make the
  # execution of this algorithm deterministic.
  benchmarks_to_shard.sort(key=lambda entry: entry.name)
  benchmark_name_to_config = {b.name: b for b in benchmarks_to_shard}
  stories_by_benchmark = {}
  for b in benchmarks_to_shard:
    stories_by_benchmark[b.name] = b.stories

  # parse the test repeat config
  benchmarks_to_repeat = {}
  stories_to_repeat = {}
  if repeat_config:
    for b in repeat_config:
      if b not in benchmark_name_to_config:
        continue
      if isinstance(repeat_config[b], int):
        repeats = min(repeat_config[b], num_shards)
        if repeats > 1:
          # repeat the whole benchmark
          benchmarks_to_repeat[b] = repeats
      else:
        # repeat specific stories of the benchmark
        for s in repeat_config[b]:
          repeats = min(repeat_config[b][s], num_shards)
          if repeats > 1:
            stories_to_repeat['%s/%s' % (b, s)] = repeats

  # Generate timing list for each way of repeating.
  # A timing list of tuples of (benchmarkname/story_name, story_duration),
  # where stories are ordered as in the benchmarks.
  benchmarks_to_run_once = [
      b for b in benchmarks_to_shard if b.name not in benchmarks_to_repeat
  ]
  one_time_story_timing_list = _gather_timing_data(benchmarks_to_run_once,
                                                   timing_data, True)

  repeated_benchmark_to_timing_list = {}
  for b in benchmarks_to_repeat:
    timing_list = _gather_timing_data([benchmark_name_to_config[b]],
                                      timing_data, True)
    repeated_benchmark_to_timing_list[b] = timing_list

  repeated_story_timing_list = {}
  for s, t in one_time_story_timing_list:
    if s in stories_to_repeat:
      repeated_story_timing_list[s] = t

  # Generate the total timing and story count
  total_time = sum(t for _, t in one_time_story_timing_list)
  total_story = len(one_time_story_timing_list)
  for benchmark, timing_list in repeated_benchmark_to_timing_list.items():
    total_time += sum(t * benchmarks_to_repeat[benchmark]
                      for s, t in timing_list)
    total_story += len(timing_list) * benchmarks_to_repeat[benchmark]
  for story, time in repeated_story_timing_list.items():
    total_time += time * (stories_to_repeat[story] - 1)
    total_story += stories_to_repeat[story] - 1
  expected_shard_time = total_time / num_shards

  sharding_map = collections.OrderedDict()
  num_stories = total_story
  min_shard_time = 0x7fffffff  # maxint
  min_shard_index = None
  max_shard_time = 0
  max_shard_index = None
  predicted_shard_timings = []
  debug_timing = collections.OrderedDict()

  current_repeat_on_benchmark = 0
  repeating_benchmark_timing_list = []
  one_time_story_timing_list.reverse()
  # The algorithm below removes all the stories from story timing lists one by
  # one and add them to the current shard until the shard's total time is
  # approximately equals to |expected_shard_time|. After that point,
  # it moves to the next shard. To make sure repeating benchmarks are not on the
  # same shard, the stories from repeated_benchmark_to_timing_list have higher
  # priority.
  # For efficient removal of story timing list's elements & to keep the
  # ordering of benchmark alphabetically sorted in the shards' assignment, we
  # reverse the |story_timing_list|.
  #TODO(crbug.com/40175917): fix extra story repeat
  for i in range(num_shards):
    if len(repeating_benchmark_timing_list) == 0:
      for benchmark, timing_list in repeated_benchmark_to_timing_list.items():
        if benchmarks_to_repeat[benchmark] > current_repeat_on_benchmark:
          repeating_benchmark_timing_list += timing_list
      repeating_benchmark_timing_list.reverse()
      current_repeat_on_benchmark += 1

    shard_name = 'shard #%i' % i
    sharding_map[str(i)] = {'benchmarks': collections.OrderedDict()}

    pre_allocated_stories = []
    pre_allocated_time = 0
    # pre_allocated_stories = all_pre_allocated_stories[i]
    # for s in pre_allocated_stories:
    #   pre_allocated_time += repeated_story_timing_list[s]
    for s, t in stories_to_repeat.items():
      if t > i:
        pre_allocated_stories.append(s)
        pre_allocated_time += repeated_story_timing_list[s]

    debug_timing[shard_name] = collections.OrderedDict()
    shard_time = pre_allocated_time
    stories_in_shard = []

    # Keep adding stories to the current shard if:
    # 1. Adding the next story does not makes the shard time further from
    # the expected;
    # Or
    # 2. The current shard is the last shard.
    while one_time_story_timing_list or repeating_benchmark_timing_list:
      if repeating_benchmark_timing_list:
        story_timing_list = repeating_benchmark_timing_list
      else:
        story_timing_list = one_time_story_timing_list

      # Add one story anyway to avoid empty shard
      current_story, current_duration = story_timing_list[-1]
      story_timing_list.pop()
      if current_story not in pre_allocated_stories:
        shard_time += current_duration
      stories_in_shard.append(current_story)
      debug_timing[shard_name][current_story] = current_duration

      if not one_time_story_timing_list and not repeating_benchmark_timing_list:
        # All stories sharded
        break

      if repeating_benchmark_timing_list:
        _, next_duration = repeating_benchmark_timing_list[-1]
      else:
        _, next_duration = one_time_story_timing_list[-1]
      if (abs(shard_time + next_duration - expected_shard_time) >
          abs(shard_time - expected_shard_time)) and i != num_shards - 1:
        # it is not the last shard and we should not add the next story
        break
    _add_benchmarks_to_shard(sharding_map, i, stories_in_shard,
                             stories_by_benchmark, benchmark_name_to_config)

    sharding_map_benchmarks = sharding_map[str(i)].get(
        'benchmarks', collections.OrderedDict())
    benchmark_sections = collections.OrderedDict()
    for benchmark, config in sharding_map_benchmarks.items():
      if 'sections' in config:
        section_list = [(s.get('begin', 0),
                         s.get('end', len(stories_by_benchmark[benchmark])))
                        for s in config['sections']]
      else:
        section_list = [(config.get('begin', 0),
                         config.get('end',
                                    len(stories_by_benchmark[benchmark])))]
      benchmark_sections[benchmark] = section_list

    for pre_allocated_story in pre_allocated_stories:
      benchmark, story = pre_allocated_story.split('/', 1)
      story_index = stories_by_benchmark[benchmark].index(story)
      if benchmark in benchmark_sections:
        benchmark_sections[benchmark].append((story_index, story_index + 1))
      else:
        benchmark_sections[benchmark] = [(story_index, story_index + 1)]

    new_benchmark_configs = collections.OrderedDict()
    for benchmark, sections in benchmark_sections.items():
      merged_sections = core.cli_utils.MergeIndexRanges(sections)
      sections_config = []
      if len(merged_sections) == 1:
        begin = merged_sections[0][0] if merged_sections[0][0] != 0 else None
        end = merged_sections[0][1] if merged_sections[0][1] != len(
            stories_by_benchmark[benchmark]) else None
        benchmark_config = {}
        if begin:
          benchmark_config['begin'] = begin
        if end:
          benchmark_config['end'] = end
      elif len(merged_sections) > 1:
        for section in merged_sections:
          sections_config.append({'begin': section[0], 'end': section[1]})
        benchmark_config = {
            'sections': sections_config,
            'abridged': benchmark_name_to_config[benchmark].abridged
        }

      abridged = benchmark_name_to_config[benchmark].abridged
      benchmark_config['abridged'] = abridged

      pageset_repeat_override = benchmark_name_to_config[
          benchmark].pageset_repeat_override
      if pageset_repeat_override:
        benchmark_config['pageset_repeat'] = pageset_repeat_override

      new_benchmark_configs[benchmark] = benchmark_config

    sharding_map[str(i)]['benchmarks'] = new_benchmark_configs
    if i != num_shards - 1:
      total_time -= shard_time
      expected_shard_time = total_time / (num_shards - i - 1)
    if shard_time > max_shard_time:
      max_shard_time = shard_time
      max_shard_index = i
    if shard_time < min_shard_time:
      min_shard_time = shard_time
      min_shard_index = i

    predicted_shard_timings.append((shard_name, shard_time))
    debug_timing[shard_name]['expected_total_time'] = shard_time

  sharding_map['extra_infos'] = collections.OrderedDict([
      ('num_stories', num_stories),
      ('predicted_min_shard_time', min_shard_time),
      ('predicted_min_shard_index', min_shard_index),
      ('predicted_max_shard_time', max_shard_time),
      ('predicted_max_shard_index', max_shard_index),
  ])

  if debug:
    sharding_map['extra_infos'].update(debug_timing)
  else:
    sharding_map['extra_infos'].update(predicted_shard_timings)
  return sharding_map


def _add_benchmarks_to_shard(sharding_map, shard_index, stories_in_shard,
    all_stories, benchmark_name_to_config):
  benchmarks = collections.OrderedDict()
  for story in stories_in_shard:
    (b, story) = story.split('/', 1)
    if b not in benchmarks:
      benchmarks[b] = []
    benchmarks[b].append(story)

  # Format the benchmark's stories by indices
  benchmarks_in_shard = collections.OrderedDict()
  executables_in_shard = collections.OrderedDict()
  crossbench_in_shard = collections.OrderedDict()
  for b in benchmarks:
    config = benchmark_name_to_config[b]
    if isinstance(config, core.bot_platforms.CrossbenchConfig):
      crossbench_in_shard[config.crossbench_name] = {
          'display_name': b,
          'arguments': config.arguments,
      }
      first_story = all_stories[b].index(benchmarks[b][0])
      last_story = all_stories[b].index(benchmarks[b][-1]) + 1
      if first_story != 0:
        crossbench_in_shard[config.crossbench_name]['begin'] = first_story
      if last_story != len(all_stories[b]):
        crossbench_in_shard[config.crossbench_name]['end'] = last_story
    elif config.is_telemetry:
      benchmarks_in_shard[b] = {}
      first_story = all_stories[b].index(benchmarks[b][0])
      last_story = all_stories[b].index(benchmarks[b][-1]) + 1
      if first_story != 0:
        benchmarks_in_shard[b]['begin'] = first_story
      if last_story != len(all_stories[b]):
        benchmarks_in_shard[b]['end'] = last_story
      benchmarks_in_shard[b]['abridged'] = benchmark_name_to_config[b].abridged
    else:
      executables_in_shard[b] = {}
      if config.flags:
        executables_in_shard[b]['arguments'] = config.flags
      executables_in_shard[b]['path'] = config.path
  sharding_map[str(shard_index)] = collections.OrderedDict()
  if benchmarks_in_shard:
    sharding_map[str(shard_index)]['benchmarks'] = benchmarks_in_shard
  if executables_in_shard:
    sharding_map[str(shard_index)]['executables'] = executables_in_shard
  if crossbench_in_shard:
    sharding_map[str(shard_index)]['crossbench'] = crossbench_in_shard


def _gather_timing_data(benchmarks_to_shard, timing_data, repeat):
  """Generates a list of story and duration in order.
  Return:
    A list of tuples of (story_name, story_duration), sorted by the order of
    benchmark name + story order within the benchmark.
  """
  timing_data_dict = {}
  for run in timing_data:
    if run['duration']:
      timing_data_dict[run['name']] = float(run['duration'])
  timing_data_list = []
  for b in benchmarks_to_shard:
    run_count = b.repeat if repeat else 1
    for s in b.stories:
      test_name = '%s/%s' % (b.name, s)
      test_duration = getattr(b, 'estimated_runtime', DEFAULT_STORY_DURATION)
      if test_name in timing_data_dict:
        test_duration = timing_data_dict[test_name] * run_count
      timing_data_list.append((test_name, test_duration))
  return timing_data_list


def test_sharding_map(
    sharding_map, benchmarks_to_shard, test_timing_data):
  story_timing_list = _gather_timing_data(
      benchmarks_to_shard, test_timing_data, False)

  story_timing_dict = dict(story_timing_list)

  results = collections.OrderedDict()
  all_stories = {}
  for b in benchmarks_to_shard:
    all_stories[b.name] = b.stories

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
