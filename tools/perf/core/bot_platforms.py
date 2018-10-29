# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import urllib

from core import benchmark_finders


_SHARD_MAP_DIR = os.path.join(os.path.dirname(__file__), 'shard_maps')


_ALL_TELEMETRY_BENCHMARKS_BY_NAMES= dict(
    (b.Name(), b) for b in benchmark_finders.GetAllBenchmarks())


_ALL_PERF_WATERFALL_TELEMETRY_BENCHMARKS = frozenset(
    benchmark_finders.GetAllPerfBenchmarks())


_ANDROID_GO_BENCHMARK_NAMES = {
    'memory.top_10_mobile',
    'system_health.memory_mobile',
    'system_health.common_mobile',
    'power.typical_10_mobile',
    'startup.mobile',
    'system_health.webview_startup',
    'v8.browsing_mobile',
    'speedometer',
    'speedometer2'
}


class PerfPlatform(object):
  def __init__(self, name, description, is_fyi=False,
               benchmarks_names_to_run=None, num_shards=None):
    self._name = name
    self._description = description
    # For sorting ignore case and "segments" in the bot name.
    self._sort_key = name.lower().replace('-', ' ')
    self._is_fyi = is_fyi
    assert num_shards
    self._num_shards = num_shards
    if benchmarks_names_to_run:
      benchmarks = []
      for benchmark_name in benchmarks_names_to_run:
        benchmarks.append(_ALL_TELEMETRY_BENCHMARKS_BY_NAMES[benchmark_name])
      benchmarks_to_run = frozenset(benchmarks)
    else:
      benchmarks_to_run = _ALL_PERF_WATERFALL_TELEMETRY_BENCHMARKS
    self._benchmarks_to_run = benchmarks_to_run

    base_file_name = name.replace(' ', '_').lower()
    self._timing_file_path = os.path.join(
        _SHARD_MAP_DIR, 'timing_data', base_file_name + '_timing.json')
    self._shards_map_file_path = os.path.join(
        _SHARD_MAP_DIR, base_file_name + '_map.json')

  def __lt__(self, other):
    if not isinstance(other, type(self)):
      return NotImplemented
    # pylint: disable=protected-access
    return self._sort_key < other._sort_key

  @property
  def num_shards(self):
    return self._num_shards

  @property
  def shards_map_file_path(self):
    return self._shards_map_file_path

  @property
  def timing_file_path(self):
    return self._timing_file_path

  @property
  def name(self):
    return self._name

  @property
  def description(self):
    return self._description

  @property
  def platform(self):
    value = self._sort_key.split(' ', 1)[0]
    return 'windows' if value == 'win' else value

  @property
  def benchmarks_to_run(self):
    return self._benchmarks_to_run

  @property
  def is_fyi(self):
    return self._is_fyi

  @property
  def buildbot_url(self):
    return ('https://ci.chromium.org/buildbot/chromium.perf/%s/' %
             urllib.quote(self._name))

# Linux
LINUX = PerfPlatform(
    'linux-perf', 'Ubuntu-14.04, 8 core, NVIDIA Quadro P400',
    num_shards=26)

# Mac
MAC_HIGH_END = PerfPlatform(
    'mac-10_13_laptop_high_end-perf',
    'MacBook Pro, Core i7 2.8 GHz, 16GB RAM, 256GB SSD, Radeon 55',
    num_shards=26)

MAC_LOW_END = PerfPlatform(
    'mac-10_12_laptop_low_end-perf',
    'MacBook Air, Core i5 1.8 GHz, 8GB RAM, 128GB SSD, HD Graphics',
    num_shards=26)

# Win
WIN_10 = PerfPlatform(
    'win-10-perf',
    'Windows Intel HD 630 towers, Core i7-7700 3.6 GHz, 16GB RAM,'
    ' Intel Kaby Lake HD Graphics 630',
    num_shards=26)

WIN_7 = PerfPlatform(
    'Win 7 Perf', 'N/A',
    num_shards=5)

WIN_7_GPU = PerfPlatform(
    'Win 7 Nvidia GPU Perf', 'N/A',
    num_shards=5)

# Android
ANDROID_GO = PerfPlatform(
    'android-go-perf', 'Android O',
    num_shards=19,
    benchmarks_names_to_run=_ANDROID_GO_BENCHMARK_NAMES)

ANDROID_NEXUS_5 = PerfPlatform(
    'Android Nexus5 Perf', 'Android KOT49H',
    num_shards=16)

ANDROID_NEXUS_5X = PerfPlatform(
    'android-nexus5x-perf', 'Android MMB29Q',
    num_shards=16)

ANDROID_NEXUS_5X_WEBVIEW = PerfPlatform(
    'Android Nexus5X WebView Perf', 'Android AOSP MOB30K',
    num_shards=16)


ANDROID_NEXUS_6_WEBVIEW = PerfPlatform(
    'Android Nexus6 WebView Perf', 'Android AOSP MOB30K',
    num_shards=8)  # Reduced from 16 per crbug.com/891848.


ALL_PLATFORMS = {
    p for p in locals().values() if isinstance(p, PerfPlatform)
}

ALL_PLATFORM_NAMES = {
    p.name for p in ALL_PLATFORMS
}
