#!/usr/bin/env vpython3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import argparse
import collections
import filecmp
import json
import multiprocessing
import os
import shutil
import sys
import tempfile
import textwrap

import cross_device_test_config

from core import path_util

path_util.AddTelemetryToPath()

from core import bot_platforms
from core import retrieve_story_timing
from core import sharding_map_generator

_SCRIPT_USAGE = """
Generate sharding maps for Telemetry benchmarks.

Every performance benchmark should be run on a same machine as long as possible
to preserve high fidelity of data monitoring. Hence in order to shard the
Telemetry benchmarks on multiple machines, we generate a JSON map that
specifies how benchmarks should be distributed on machines. There is one
sharding JSON map for every builder in the perf & perf.fyi waterfalls which are
specified by PerfPlatform classes in //tools/perf/core/bot_platforms.py.

Generating these JSON maps depends on how many Telemetry benchmarks
actually exist at the time. Because of this, CLs to generate the JSON maps
should never be automatically reverted, since the reverted state of the JSON map
files may not match with the true state of world.

"""


def GetParser():
  parser = argparse.ArgumentParser(
    description=_SCRIPT_USAGE, formatter_class=argparse.RawTextHelpFormatter
  )

  subparsers = parser.add_subparsers(
    required=True, metavar='{update,update-timing,deschedule,validate}'
  )

  parser_update = subparsers.add_parser('update', help='Update the shard maps.')
  parser_update.add_argument(
    '--use-existing-timing-data',
    '-o',
    action='store_true',
    help=(
      'Whether to reuse existing builder timing data (stored in '
      '//tools/perf/core/shard_maps/timing_data/) and skip the step of '
      'fetching the most recent timing data from test results server. '
      'This flag is default to False. One typically uses this option '
      'when they need to fix the timing data to debug sharding '
      'generation.'
    ),
    default=False,
  )
  _AddBuilderPlatformSelectionArgs(parser_update)
  parser.add_argument(
    '--debug',
    action='store_true',
    help=(
      'Whether to include detailed debug info of the sharding map in the '
      'shard maps.'
    ),
    default=False,
  )
  parser_update.set_defaults(func=_UpdateShardsForBuilders)

  parser_update_timing = subparsers.add_parser(
    'update-timing',
    help='Update the timing data that is used to create the shard maps, '
    'but don\'t update the shard maps themselves.',
  )
  _AddBuilderPlatformSelectionArgs(parser_update_timing)
  parser_update_timing.add_argument(
    '--filter-only',
    action='store_true',
    help='Do not grab new data from bigquery but instead simply filter '
    'the existing data to reflect some change in the benchmark (for example '
    'if the benchmark was switched to abridged mode on some platform or if '
    'a story was removed from the benchmark.)',
  )
  parser_update_timing.set_defaults(func=_UpdateTimingDataCommand)

  parser_deschedule = subparsers.add_parser(
    'deschedule',
    help=(
      'After you deschedule one or more '
      'benchmarks by deleting from tools/perf/benchmarks or by editing '
      'bot_platforms.py, use this script to deschedule the '
      'benchmark(s) without impacting the sharding for other benchmarks.'
    ),
  )
  parser_deschedule.set_defaults(func=_DescheduleBenchmark)

  parser_validate = subparsers.add_parser(
    'validate',
    help=(
      'Validate that the shard maps match up with the benchmarks and '
      'bot_platforms.py.'
    ),
  )
  parser_validate.set_defaults(func=_ValidateShardMaps)

  return parser


def _AddBuilderPlatformSelectionArgs(parser):
  builder_selection = parser.add_mutually_exclusive_group()
  builder_selection.add_argument(
    '--builders',
    '-b',
    action='append',
    help=('The builder names to use.'),
    default=[],
    choices=bot_platforms.ALL_PLATFORM_NAMES,
  )
  builder_selection.add_argument(
    '--waterfall',
    '-w',
    choices=['perf', 'perf-fyi', 'all'],
    default=None,
    help=(
      'The name of waterfall whose builders should be used. If not '
      'specified, use all perf builders by default'
    ),
  )


def _DumpJson(data, output_path):
  with open(output_path, 'w', newline='') as output_file:
    json.dump(data, output_file, indent=4, separators=(',', ': '))
    output_file.write('\n')


TIMING_SOURCE_MEASURED = 'measured'
TIMING_SOURCE_ESTIMATE = 'estimate'
# Number of days of results used for timing data, for all benchmark types. Long
# enough that low-cadence testers, or testers that were down for a few days,
# still have measured data.
TIMING_NUM_LAST_DAYS = 14
# Minimum number of measured runs (within TIMING_NUM_LAST_DAYS) before an
# executable or Crossbench duration replaces its estimated_runtime.
MIN_MEASURED_RUNS = 10


def MergeTimingData(builder, story_timing_data, run_total_timing_data):
  """Builds the timing data used to shard |builder|'s benchmarks.

  Telemetry stories are reported to ResultDB as '<benchmark>/<story>' and keep
  their measured average duration from |story_timing_data|.

  Executables and Crossbench benchmarks are reported under their bare name
  (e.g. 'dawn_perf_tests'), so their measured per-run durations are taken from
  |run_total_timing_data| and renamed to the keys the sharding map generator
  looks up: '<executable>/_gtest_' and '<crossbench>/<story>'. Executables
  without measured data fall back to their estimated_runtime. Crossbench
  benchmarks without measured data are left out, so the sharding map generator
  falls back to their estimated_runtime. A measurement needs a non-zero
  duration and at least MIN_MEASURED_RUNS runs to be used.

  An executable's estimated_runtime covers its whole run: executables run once
  per shard (their repeat count is not passed to them), so the estimate is not
  scaled by repeat.

  Every entry records whether its duration was measured or estimated, and
  measured executable and Crossbench entries record their number of runs.

  Args:
    builder: the bot_platforms._PerfPlatform to generate timing data for.
    story_timing_data: list of {'name', 'duration'} dicts with the average
      duration of each test ID (retrieve_story_timing.
      FetchAverageStoryTimingData).
    run_total_timing_data: list of {'name', 'duration', 'num_runs'} dicts with
      the average per-run total duration of each test ID (retrieve_story_timing.
      FetchAverageTestRunTotalData).

  Returns:
    A list of {'name', 'duration', 'source'} dicts, plus 'num_runs' on
    measured executable and Crossbench entries.
  """
  executables = {e.name: e for e in builder.executables}
  crossbench_stories = {c.name: c.stories for c in builder.crossbench}
  non_telemetry_names = set(executables) | set(crossbench_stories)

  merged = []
  for entry in story_timing_data:
    if entry['name'] in non_telemetry_names:
      continue
    merged.append(dict(entry, source=TIMING_SOURCE_MEASURED))

  # Zero or missing durations (e.g. results that don't report a duration) and
  # measurements from too few runs are not used.
  run_totals = {}
  for entry in run_total_timing_data:
    if entry['name'] not in non_telemetry_names:
      continue
    duration = float(entry.get('duration') or 0)
    num_runs = int(entry.get('num_runs') or 0)
    if duration > 0 and num_runs >= MIN_MEASURED_RUNS:
      run_totals[entry['name']] = (duration, num_runs)

  for name in sorted(executables):
    entry = {'name': name + '/' + bot_platforms.GTEST_STORY_NAME}
    if name in run_totals:
      duration, num_runs = run_totals[name]
      entry.update(source=TIMING_SOURCE_MEASURED, num_runs=num_runs)
    else:
      duration = float(executables[name].estimated_runtime)
      entry.update(source=TIMING_SOURCE_ESTIMATE)
    entry['duration'] = str(duration)
    merged.append(entry)

  for name in sorted(crossbench_stories):
    if name not in run_totals:
      continue
    duration, num_runs = run_totals[name]
    stories = crossbench_stories[name]
    if not stories:
      continue
    # One Crossbench result covers all of its stories, so split the measured
    # duration evenly between them.
    story_duration = duration / len(stories)
    for story in stories:
      merged.append(
        {
          'duration': str(story_duration),
          'name': name + '/' + story,
          'source': TIMING_SOURCE_MEASURED,
          'num_runs': num_runs,
        }
      )
  return merged


def _LoadTimingData(args):
  builder, timing_file_path = args
  story_timing_data = retrieve_story_timing.FetchAverageStoryTimingData(
    configurations=[builder.name], num_last_days=TIMING_NUM_LAST_DAYS
  )
  run_total_timing_data = []
  if builder.executables or builder.crossbench:
    run_total_timing_data = retrieve_story_timing.FetchAverageTestRunTotalData(
      configurations=[builder.name],
      num_last_days=TIMING_NUM_LAST_DAYS,
    )
  data = MergeTimingData(builder, story_timing_data, run_total_timing_data)
  _DumpJson(data, timing_file_path)
  print('Finished retrieving story timing data for %s' % repr(builder.name))


def _source_filepath(posix_path):
  return os.path.join(path_util.GetChromiumSrcDir(), *posix_path.split('/'))


def GenerateShardMap(builder, num_of_shards, debug=False):
  timing_data = []
  if builder:
    with open(builder.timing_file_path) as f:
      timing_data = json.load(f)
  benchmarks_to_shard = (
    list(builder.benchmark_configs)
    + list(builder.executables)
    + list(builder.crossbench)
  )
  repeat_config = cross_device_test_config.TARGET_DEVICES.get(builder.name, {})
  sharding_map = sharding_map_generator.generate_sharding_map(
    benchmarks_to_shard,
    timing_data,
    num_shards=num_of_shards,
    debug=debug,
    repeat_config=repeat_config,
  )
  return sharding_map


def _GenerateShardMapJson(builder, num_of_shards, output_path, debug):
  sharding_map = GenerateShardMap(builder, num_of_shards, debug)
  _DumpJson(sharding_map, output_path)


def _PromptWarning():
  message = (
    'This will regenerate the sharding maps for perf benchmarks. '
    'Note that this will shuffle all the benchmarks on the shards, '
    'which can cause false regressions. In general this operation '
    'should only be done when the shards are too unbalanced or when '
    'benchmarks are added/removed. '
    'In addition, this is a tricky operation and should '
    'always be reviewed by Benchmarking '
    'team members. Upon landing the CL to update the shard maps, '
    'please notify Chromium perf sheriffs in '
    'perf-sheriffs@chromium.org and put a warning about expected '
    'false regressions in your CL '
    'description'
  )
  print(textwrap.fill(message, 70), '\n')
  answer = input("Enter 'y' to continue: ")
  if answer != 'y':
    print('Abort updating shard maps for benchmarks on perf waterfall')
    sys.exit(0)


def _UpdateTimingDataCommand(args):
  builders = _GetBuilderPlatforms(args.builders, args.waterfall)
  if not args.filter_only:
    _UpdateTimingData(builders)
  for builder in builders:
    _FilterTimingData(builder)


def _FilterTimingData(builder, output_path=None):
  output_path = output_path or builder.timing_file_path
  with open(builder.timing_file_path) as f:
    timing_dataset = json.load(f)
  story_full_names = set()
  for benchmark_config in builder.benchmark_configs:
    for story in benchmark_config.exhaustive_stories:
      story_full_names.add('/'.join([benchmark_config.name, story]))
  # When benchmarks are abridged or stories are removed, we want that
  # to be reflected in the timing data right away.
  executable_story_names = [
    e.name + '/' + bot_platforms.GTEST_STORY_NAME for e in builder.executables
  ]
  crossbench_story_names = [
    c.name + '/' + story for c in builder.crossbench for story in c.stories
  ]
  timing_dataset = [
    point
    for point in timing_dataset
    if (
      str(point['name']) in story_full_names
      or str(point['name']) in executable_story_names
      or str(point['name']) in crossbench_story_names
    )
  ]
  _DumpJson(timing_dataset, output_path)


def _UpdateTimingData(builders):
  print('Updating shards timing data. May take a while...')
  load_timing_args = []
  for b in builders:
    load_timing_args.append((b, b.timing_file_path))
  p = multiprocessing.Pool(len(load_timing_args))
  # Use map_async to work around python bug. See crbug.com/1026004.
  p.map_async(_LoadTimingData, load_timing_args).get(12 * 60 * 60)


def _GetBuilderPlatforms(builders, waterfall):
  """Get a list of PerfBuilder objects for the given builders or waterfall.

  Otherwise, just return all platforms.
  """
  if builders:
    return {b for b in bot_platforms.ALL_PLATFORMS if b.name in builders}
  if waterfall == 'perf':
    platforms = bot_platforms.OFFICIAL_PLATFORMS
  elif waterfall == 'perf-fyi':
    platforms = bot_platforms.FYI_PLATFORMS
  else:
    platforms = bot_platforms.ALL_PLATFORMS
  return {p for p in platforms if not p.pinpoint_only}


def _UpdateShardsForBuilders(args):
  _PromptWarning()
  builders = _GetBuilderPlatforms(args.builders, args.waterfall)
  if not args.use_existing_timing_data:
    _UpdateTimingData(builders)
  for b in builders:
    _GenerateShardMapJson(b, b.num_shards, b.shards_map_file_path, args.debug)
    print('Updated sharding map for %s' % repr(b.name))


def _DescheduleBenchmark(args):
  """Remove benchmarks from the shard maps without re-sharding."""
  del args
  builders = bot_platforms.ALL_PLATFORMS
  for b in builders:
    benchmarks_to_keep = set(
      benchmark.Name() for benchmark in b.benchmarks_to_run
    )
    executables_to_keep = set(executable.name for executable in b.executables)
    with open(b.shards_map_file_path, 'r') as f:
      if not os.path.exists(b.shards_map_file_path):
        continue
      shards_map = json.load(f, object_pairs_hook=collections.OrderedDict)
      for shard, shard_map in shards_map.items():
        if shard == 'extra_infos':
          break
        benchmarks = shard_map.get('benchmarks', dict())
        for benchmark in list(benchmarks.keys()):
          if benchmark not in benchmarks_to_keep:
            del benchmarks[benchmark]
        executables = shard_map.get('executables', dict())
        for executable in executables.keys():
          if executable not in executables_to_keep:
            del executables[executable]
    os.remove(b.shards_map_file_path)
    _DumpJson(shards_map, b.shards_map_file_path)
  print('done.')


def _ParseBenchmarks(shard_map_path):
  if not os.path.exists(shard_map_path):
    raise RuntimeError(
      'Platform does not have a shard map at %s.' % shard_map_path
    )
  all_benchmarks = set()
  with open(shard_map_path) as f:
    shard_map = json.load(f)
  for shard, benchmarks_in_shard in shard_map.items():
    if "extra_infos" in shard:
      continue
    if benchmarks := benchmarks_in_shard.get('benchmarks'):
      all_benchmarks |= set(benchmarks.keys())
    if executables := benchmarks_in_shard.get('executables'):
      all_benchmarks |= set(executables.keys())
    if crossbench := benchmarks_in_shard.get('crossbench'):
      all_benchmarks |= set(crossbench.keys())
  return frozenset(all_benchmarks)


def _ValidateShardMaps(args):
  """Validate that the shard maps, csv files, etc. are consistent."""
  del args
  errors = []

  tempdir = tempfile.mkdtemp()
  try:
    builders = _GetBuilderPlatforms(builders=None, waterfall='all')
    for builder in builders:
      output_file = os.path.join(
        tempdir, os.path.basename(builder.timing_file_path)
      )
      _FilterTimingData(builder, output_file)
      if not filecmp.cmp(builder.timing_file_path, output_file):
        errors.append(
          '{timing_data} is not up to date. Please run '
          '`./generate_perf_sharding.py update-timing --filter-only` '
          'to regenerate it.'.format(timing_data=builder.timing_file_path)
        )
  finally:
    shutil.rmtree(tempdir)

  # Check that bot_platforms.py matches the actual shard maps
  for platform in bot_platforms.ALL_PLATFORMS:
    if platform.pinpoint_only:
      continue
    platform_benchmark_names = {
      b.name
      for b in (
        platform.benchmark_configs | platform.executables | platform.crossbench
      )
    }
    shard_map_benchmark_names = _ParseBenchmarks(platform.shards_map_file_path)
    new_benchmarks = platform_benchmark_names - shard_map_benchmark_names
    for benchmark in new_benchmarks:
      errors.append(
        f'Benchmark {benchmark} is supposed to be scheduled on platform '
        f'{platform.name} according to '
        f'schedule/{benchmark}.csv, but it is not yet scheduled.'
        f'Please regenerate the shard map {platform.shards_map_file_path}'
        f'with `{__file__} update`'
      )
    stale_benchmarks = shard_map_benchmark_names - platform_benchmark_names
    for benchmark in stale_benchmarks:
      errors.append(
        f'Benchmark {benchmark} is scheduled on shard map '
        f'{platform.shards_map_file_path}, but '
        f'schedule/{benchmark}.csv, '
        'says that it should not be on that shard map. '
        'If that is the case, you can use '
        f'`{__file__} deschedule` to deschedule the benchmark '
        'from the shard map.'
      )

  for error in errors:
    print('*', error, '\n', file=sys.stderr)
  if errors:
    return 1
  return 0


def main():
  parser = GetParser()
  options = parser.parse_args()
  return options.func(options)


if __name__ == '__main__':
  sys.exit(main())
