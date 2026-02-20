#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(cbruni): Remove this script once migration is completed.

import json
import sys
from pathlib import Path

# Add tools/perf to sys.path
sys.path.append(str(Path(__file__).resolve().parents[1]))

from core import path_util

path_util.AddTelemetryToPath()

from core import bot_platforms


def serialize_benchmark_config(config):
  return {
      'name': config.name,
      'abridged': config.abridged,
      'repeat': config.repeat,
      'is_telemetry': config.is_telemetry,
      'extra_flags': config.extra_flags,
  }


def serialize_executable_config(config):
  return {
      'name': config.name,
      'path': config.path,
      'flags': config.flags,
      'extra_flags': config.extra_flags,
      'estimated_runtime': config.estimated_runtime,
      'is_telemetry': config.is_telemetry,
      'repeat': config.repeat,
      'stories': config.stories,
      'abridged': config.abridged,
  }


def serialize_crossbench_config(config):
  return {
      'name': config.name,
      'crossbench_name': config.crossbench_name,
      'estimated_runtime': config.estimated_runtime,
      'stories': config.stories,
      'arguments': config.arguments,
      'repeat': config.repeat,
      'extra_flags': config.extra_flags,
  }


def serialize_platform(platform):
  return {
      'name':
      platform.name,
      'description':
      platform.description,
      'platform_os':
      platform.platform,
      'num_shards':
      platform.num_shards,
      'is_fyi':
      platform.is_fyi,
      'is_official':
      platform.is_official,
      'run_reference_build':
      platform.run_reference_build,
      'pinpoint_only':
      platform.pinpoint_only,
      'builder_url':
      platform.builder_url,
      'timing_file_path':
      platform.timing_file_path,
      'shards_map_file_path':
      platform.shards_map_file_path,
      'benchmark_configs': [
          serialize_benchmark_config(b)
          for b in sorted(platform.benchmark_configs, key=lambda x: x.name)
      ],
      'executables': [
          serialize_executable_config(e)
          for e in sorted(platform.executables, key=lambda x: x.name)
      ],
      'crossbench': [
          serialize_crossbench_config(c)
          for c in sorted(platform.crossbench, key=lambda x: x.name)
      ],
  }


def main():
  platforms_data = {}
  sorted_platforms = sorted(bot_platforms.ALL_PLATFORMS, key=lambda p: p.name)

  for platform in sorted_platforms:
    platforms_data[platform.name] = serialize_platform(platform)

  output_path = Path(__file__).resolve().parent / 'bot_platforms.json'
  with output_path.open('w', encoding='utf-8') as f:
    json.dump(platforms_data, f, indent=2, sort_keys=True)

  print(f"Serialized {len(platforms_data)} platforms to {output_path}")


if __name__ == '__main__':
  main()
