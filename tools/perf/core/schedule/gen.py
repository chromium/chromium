#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(cbruni): Remove this script once migration is completed.

import sys
import csv
import shlex
from collections import defaultdict
from pathlib import Path

# Add tools/perf to sys.path
sys.path.append(str(Path(__file__).resolve().parents[2]))

from core import path_util

path_util.AddTelemetryToPath()

from core import bot_platforms
from cross_device_test_config import TARGET_DEVICES


def get_shard_count(benchmark_name, bot_name):
  if num_shards := TARGET_DEVICES.get(bot_name, {}).get(benchmark_name):
    if isinstance(num_shards, int):
      return num_shards
  return 1

# For now we include all non-empty flags, except for these manually
# known presets.
KNOWN_PRESET_FLAGS = ("--js-flags=--turbolev-future",
                      "--extra-browser-args=--force-renderer-accessibility")

def main():
  output_dir = Path(__file__).resolve().parent
  output_dir.mkdir(parents=True, exist_ok=True)

  benchmark_to_bots = defaultdict(list)

  # Iterate through all platforms
  for platform in bot_platforms.ALL_PLATFORMS:
    bot_name = platform.name

    # Collect all configurations (benchmarks, crossbench, executables)
    all_configs = []
    all_configs.extend(platform.benchmark_configs)
    all_configs.extend(platform.crossbench)
    all_configs.extend(platform.executables)

    for config in platform.crossbench:
      benchmark_to_bots[config.name].append({
          "bot":
          bot_name,
          "repeat":
          config.repeat,
          "shard":
          get_shard_count(config.name, bot_name),
          "flags":
          shlex.join(config.arguments)
      })

    for config in platform.executables:
      benchmark_to_bots[config.name].append({
          "bot":
          bot_name,
          "repeat":
          config.repeat,
          "shard":
          get_shard_count(config.name, bot_name),
          "estimated_runtime":
          config.estimated_runtime,
          "flags":
          shlex.join(config.extra_flags)
      })

    for config in platform.benchmark_configs:
      benchmark_to_bots[config.name].append({
          "bot":
          bot_name,
          "repeat":
          config.repeat,
          "shard":
          get_shard_count(config.name, bot_name),
          "abridged":
          config.abridged,
      })

  # Write the CSV files
  count = 0
  for benchmark, bots_data in benchmark_to_bots.items():
    bots_data.sort(key=lambda x: x["bot"])
    column_names = bots_data[0].keys()

    # Check for constant columns
    same_values_warnings = {}
    skip_same_value_columns = set()
    for column in column_names:
      first_value = bots_data[0].get(column)
      if all(bot_data.get(column) == first_value for bot_data in bots_data):
        same_values_warnings[column] = first_value

    if 'flags' in same_values_warnings:
      flag_value = same_values_warnings['flags']
      if not flag_value or flag_value in KNOWN_PRESET_FLAGS:
        del same_values_warnings['flags']
        skip_same_value_columns.add('flags')

    if 'abridged' in same_values_warnings and not same_values_warnings[
        'abridged']:
      del same_values_warnings['abridged']
      skip_same_value_columns.add('abridged')

    if 'estimated_runtime' in same_values_warnings:
      del same_values_warnings['estimated_runtime']
      skip_same_value_columns.add('estimated_runtime')

    if len(bots_data) > 1 and same_values_warnings:
      print(f"{benchmark}: Same values for all rows")
      for column, value in same_values_warnings.items():
        print(f"  {column}: {value}")

    if skip_same_value_columns:
      # Remove unused columns, they can be defined in the config objects
      # as default values.
      print(benchmark, skip_same_value_columns)
      for bot_data in bots_data:
        for column in skip_same_value_columns:
          del bot_data[column]

    # Ensure flags as last value
    for bot_data in bots_data:
      if 'flags' in bot_data:
        bot_data['flags'] = bot_data.pop('flags')

    column_names = bots_data[0].keys()
    filepath = output_dir / f"{benchmark}.csv"
    with filepath.open('w+', newline='', encoding='utf-8') as f:
      writer = csv.DictWriter(f, fieldnames=column_names, lineterminator='\n')
      writer.writeheader()
      for bot_data in bots_data:
        writer.writerow(bot_data)
    count += 1

  print(f"Created {count} files in '{output_dir}/'")


if __name__ == '__main__':
  main()
