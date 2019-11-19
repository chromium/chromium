# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import urllib

from core import benchmark_finders


_SHARD_MAP_DIR = os.path.join(os.path.dirname(__file__), 'shard_maps')

_ALL_BENCHMARKS_BY_NAMES = dict(
    (b.Name(), b) for b in benchmark_finders.GetAllBenchmarks())

OFFICIAL_BENCHMARKS = frozenset(
    b for b in benchmark_finders.GetOfficialBenchmarks()
    if not b.Name().startswith('UNSCHEDULED_'))
CONTRIB_BENCHMARKS = frozenset(benchmark_finders.GetContribBenchmarks())
ALL_SCHEDULEABLE_BENCHMARKS = OFFICIAL_BENCHMARKS | CONTRIB_BENCHMARKS

def _IsPlatformSupported(benchmark, platform):
    supported = benchmark.GetSupportedPlatformNames(
        benchmark.SUPPORTED_PLATFORMS)
    return 'all' in supported or platform in supported


class PerfPlatform(object):
  def __init__(self, name, description, benchmark_configs,
               num_shards, platform_os, is_fyi=False):
    self._name = name
    self._description = description
    self._platform_os = platform_os
    # For sorting ignore case and "segments" in the bot name.
    self._sort_key = name.lower().replace('-', ' ')
    self._is_fyi = is_fyi
    assert num_shards
    self._num_shards = num_shards
    # pylint: disable=redefined-outer-name
    self._benchmark_configs = frozenset([
        b for b in benchmark_configs if
          _IsPlatformSupported(b.benchmark, self._platform_os)])
    # pylint: enable=redefined-outer-name

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
    return self._platform_os

  @property
  def benchmarks_to_run(self):
    # TODO(crbug.com/965158): Deprecate this in favor of benchmark_configs
    # as part of change to make sharding scripts accommodate abridged
    # benchmarks.
    return frozenset({b.benchmark for b in self._benchmark_configs})

  @property
  def benchmark_configs(self):
    return self._benchmark_configs

  @property
  def is_fyi(self):
    return self._is_fyi

  @property
  def builder_url(self):
    return ('https://ci.chromium.org/p/chrome/builders/ci/%s' %
             urllib.quote(self._name))


class BenchmarkConfig(object):
  def __init__(self, benchmark, abridged):
    """A configuration for a benchmark that helps decide how to shard it.

    Args:
      benchmark: the benchmark.Benchmark object.
      abridged: True if the benchmark should be abridged so fewer stories
        are run, and False if the whole benchmark should be run.
    """
    self.benchmark = benchmark
    self.abridged = abridged

  @property
  def name(self):
    return self.benchmark.Name()

# Global |benchmarks| is convenient way to keep BenchmarkConfig objects
# unique, which allows us to use set subtraction below.
benchmarks = {b.Name(): {True: BenchmarkConfig(b, abridged=True),
                         False: BenchmarkConfig(b, abridged=False)}
              for b in ALL_SCHEDULEABLE_BENCHMARKS}

def _GetBenchmarkConfig(benchmark_name, abridged=False):
  return benchmarks[benchmark_name][abridged]

OFFICIAL_BENCHMARK_CONFIGS = frozenset(
    _GetBenchmarkConfig(b.Name()) for b in OFFICIAL_BENCHMARKS)
# TODO(crbug.com/965158): Remove OFFICIAL_BENCHMARK_NAMES once sharding
# scripts are no longer using it.
OFFICIAL_BENCHMARK_NAMES = frozenset(
    b.name for b in OFFICIAL_BENCHMARK_CONFIGS)
_DL_BENCHMARK_CONFIGS = frozenset([_GetBenchmarkConfig(
    'blink_perf.display_locking')])
_JETSTREAM2 = frozenset([_GetBenchmarkConfig('jetstream2')])

_OFFICIAL_EXCEPT_DISPLAY_LOCKING = (
    OFFICIAL_BENCHMARK_CONFIGS - _DL_BENCHMARK_CONFIGS)
_OFFICIAL_EXCEPT_JETSTREAM2 = (
    OFFICIAL_BENCHMARK_CONFIGS - _JETSTREAM2)
_OFFICIAL_EXCEPT_DISPLAY_LOCKING_JETSTREAM2 = (
    OFFICIAL_BENCHMARK_CONFIGS - _DL_BENCHMARK_CONFIGS - _JETSTREAM2)

_LINUX_BENCHMARK_CONFIGS = _OFFICIAL_EXCEPT_DISPLAY_LOCKING
_MAC_HIGH_END_BENCHMARK_CONFIGS = _OFFICIAL_EXCEPT_DISPLAY_LOCKING
_MAC_LOW_END_BENCHMARK_CONFIGS = _OFFICIAL_EXCEPT_JETSTREAM2
_WIN_10_BENCHMARK_CONFIGS = _OFFICIAL_EXCEPT_DISPLAY_LOCKING
_WIN_10_LOW_END_HP_CANDIDATE_BENCHMARK_CONFIGS = frozenset([
    _GetBenchmarkConfig('v8.browsing_desktop')])
_WIN_7_BENCHMARK_CONFIGS = (_OFFICIAL_EXCEPT_DISPLAY_LOCKING_JETSTREAM2 -
                          frozenset([_GetBenchmarkConfig('rendering.desktop')]))
_WIN_7_GPU_BENCHMARK_CONFIGS = _OFFICIAL_EXCEPT_DISPLAY_LOCKING_JETSTREAM2
_ANDROID_GO_BENCHMARK_CONFIGS = frozenset([
    _GetBenchmarkConfig('system_health.memory_mobile'),
    _GetBenchmarkConfig('system_health.common_mobile'),
    _GetBenchmarkConfig('startup.mobile'),
    _GetBenchmarkConfig('system_health.webview_startup'),
    _GetBenchmarkConfig('v8.browsing_mobile'),
    _GetBenchmarkConfig('speedometer'),
    _GetBenchmarkConfig('speedometer2')])
_ANDROID_GO_WEBVIEW_BENCHMARK_CONFIGS = _ANDROID_GO_BENCHMARK_CONFIGS
_ANDROID_NEXUS_5_BENCHMARK_CONFIGS = _OFFICIAL_EXCEPT_DISPLAY_LOCKING_JETSTREAM2
_ANDROID_NEXUS_5X_BENCHMARK_CONFIGS = _OFFICIAL_EXCEPT_JETSTREAM2
_ANDROID_NEXUS_5X_WEBVIEW_BENCHMARK_CONFIGS = (
    _OFFICIAL_EXCEPT_DISPLAY_LOCKING_JETSTREAM2)
_ANDROID_NEXUS_6_WEBVIEW_BENCHMARK_CONFIGS = (
    _OFFICIAL_EXCEPT_DISPLAY_LOCKING_JETSTREAM2)
_ANDROID_PIXEL2_BENCHMARK_CONFIGS = _OFFICIAL_EXCEPT_DISPLAY_LOCKING
_ANDROID_PIXEL2_WEBVIEW_BENCHMARK_CONFIGS = (
    _OFFICIAL_EXCEPT_DISPLAY_LOCKING_JETSTREAM2)
_ANDROID_PIXEL2_WEBLAYER_BENCHMARK_CONFIGS = frozenset([
    _GetBenchmarkConfig('system_health.common_mobile')])
_ANDROID_NEXUS5X_FYI_BENCHMARK_CONFIGS = frozenset([
    _GetBenchmarkConfig('heap_profiling.mobile.disabled'),
    _GetBenchmarkConfig('heap_profiling.mobile.native'),
    _GetBenchmarkConfig('heap_profiling.mobile.pseudo')])
_ANDROID_PIXEL2_FYI_BENCHMARK_CONFIGS = frozenset([
    _GetBenchmarkConfig('v8.browsing_mobile'),
    _GetBenchmarkConfig('system_health.memory_mobile'),
    _GetBenchmarkConfig('system_health.common_mobile'),
    _GetBenchmarkConfig('startup.mobile'),
    _GetBenchmarkConfig('speedometer2'),
    _GetBenchmarkConfig('octane'),
    _GetBenchmarkConfig('jetstream')])
_CHROMEOS_KEVIN_FYI_BENCHMARK_CONFIGS = frozenset([
    _GetBenchmarkConfig('rendering.desktop')])

# Linux
LINUX = PerfPlatform(
    'linux-perf', 'Ubuntu-14.04, 8 core, NVIDIA Quadro P400',
    _LINUX_BENCHMARK_CONFIGS, 26, 'linux')

# Mac
MAC_HIGH_END = PerfPlatform(
    'mac-10_13_laptop_high_end-perf',
    'MacBook Pro, Core i7 2.8 GHz, 16GB RAM, 256GB SSD, Radeon 55',
    _MAC_HIGH_END_BENCHMARK_CONFIGS, 26, 'mac')
MAC_LOW_END = PerfPlatform(
    'mac-10_12_laptop_low_end-perf',
    'MacBook Air, Core i5 1.8 GHz, 8GB RAM, 128GB SSD, HD Graphics',
    _MAC_LOW_END_BENCHMARK_CONFIGS, 26, 'mac')

# Win
WIN_10 = PerfPlatform(
    'win-10-perf',
    'Windows Intel HD 630 towers, Core i7-7700 3.6 GHz, 16GB RAM,'
    ' Intel Kaby Lake HD Graphics 630', _WIN_10_BENCHMARK_CONFIGS,
    26, 'win')
WIN_7 = PerfPlatform(
    'Win 7 Perf', 'N/A', _WIN_7_BENCHMARK_CONFIGS,
    5, 'win')
WIN_7_GPU = PerfPlatform(
    'Win 7 Nvidia GPU Perf', 'N/A', _WIN_7_GPU_BENCHMARK_CONFIGS,
    5, 'win')

# Android
ANDROID_GO = PerfPlatform(
    'android-go-perf', 'Android O (gobo)', _ANDROID_GO_BENCHMARK_CONFIGS,
    19, 'android')
ANDROID_GO_WEBVIEW = PerfPlatform(
    'android-go_webview-perf', 'Android OPM1.171019.021 (gobo)',
    _ANDROID_GO_WEBVIEW_BENCHMARK_CONFIGS, 13, 'android')
ANDROID_NEXUS_5 = PerfPlatform(
    'Android Nexus5 Perf', 'Android KOT49H', _ANDROID_NEXUS_5_BENCHMARK_CONFIGS,
    16, 'android')
ANDROID_NEXUS_5X = PerfPlatform(
    'android-nexus5x-perf', 'Android MMB29Q',
    _ANDROID_NEXUS_5X_BENCHMARK_CONFIGS,
    10, # Reduced from 16 per crbug.com/1014120.
    'android')
ANDROID_NEXUS_5X_WEBVIEW = PerfPlatform(
    'Android Nexus5X WebView Perf', 'Android AOSP MOB30K',
    _ANDROID_NEXUS_5X_WEBVIEW_BENCHMARK_CONFIGS, 16, 'android')
ANDROID_NEXUS_6_WEBVIEW = PerfPlatform(
    'Android Nexus6 WebView Perf', 'Android AOSP MOB30K',
    _ANDROID_NEXUS_6_WEBVIEW_BENCHMARK_CONFIGS,
    12,  # Reduced from 16 per crbug.com/891848.
    'android')
ANDROID_PIXEL2 = PerfPlatform(
    'android-pixel2-perf', 'Android OPM1.171019.021',
    _ANDROID_PIXEL2_BENCHMARK_CONFIGS, 35, 'android')
ANDROID_PIXEL2_WEBVIEW = PerfPlatform(
    'android-pixel2_webview-perf', 'Android OPM1.171019.021',
    _ANDROID_PIXEL2_WEBVIEW_BENCHMARK_CONFIGS, 28, 'android')
ANDROID_PIXEL2_WEBLAYER = PerfPlatform(
    'android-pixel2_weblayer-perf', 'Android OPM1.171019.021',
    _ANDROID_PIXEL2_WEBLAYER_BENCHMARK_CONFIGS, 4, 'android')
# FYI bots
WIN_10_LOW_END_HP_CANDIDATE = PerfPlatform(
    'win-10_laptop_low_end-perf_HP-Candidate', 'HP 15-BS121NR Laptop Candidate',
    _WIN_10_LOW_END_HP_CANDIDATE_BENCHMARK_CONFIGS,
    1, 'win', is_fyi=True)
ANDROID_NEXUS5X_PERF_FYI =  PerfPlatform(
    'android-nexus5x-perf-fyi', 'Android MMB29Q',
    _ANDROID_NEXUS5X_FYI_BENCHMARK_CONFIGS,
    3, 'android', is_fyi=True)
ANDROID_PIXEL2_PERF_FYI = PerfPlatform(
    'android-pixel2-perf-fyi', 'Android OPM1.171019.021',
    _ANDROID_PIXEL2_FYI_BENCHMARK_CONFIGS,
    4, 'android', is_fyi=True)
CHROMEOS_KEVIN_PERF_FYI = PerfPlatform(
    'chromeos-kevin-perf-fyi', '',
    _CHROMEOS_KEVIN_FYI_BENCHMARK_CONFIGS,
    4, 'chromeos', is_fyi=True)

# TODO(crbug.com/902089): Add linux-perf-fyi once the bot is configured to use
# the sharding map.

ALL_PLATFORMS = {
    p for p in locals().values() if isinstance(p, PerfPlatform)
}

FYI_PLATFORMS = {
    p for p in ALL_PLATFORMS if p.is_fyi
}

OFFICIAL_PLATFORMS = {
    p for p in ALL_PLATFORMS if not p.is_fyi
}

ALL_PLATFORM_NAMES = {
    p.name for p in ALL_PLATFORMS
}

OFFICIAL_PLATFORM_NAMES = {
    p.name for p in OFFICIAL_PLATFORMS
}
