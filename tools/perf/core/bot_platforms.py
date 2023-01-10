# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import six.moves.urllib.parse  # pylint: disable=import-error

from core import benchmark_finders
from core import benchmark_utils

from telemetry.story import story_filter


_SHARD_MAP_DIR = os.path.join(os.path.dirname(__file__), 'shard_maps')

_ALL_BENCHMARKS_BY_NAMES = dict(
    (b.Name(), b) for b in benchmark_finders.GetAllBenchmarks())

OFFICIAL_BENCHMARKS = frozenset(
    b for b in benchmark_finders.GetOfficialBenchmarks()
    if not b.Name().startswith('UNSCHEDULED_'))
CONTRIB_BENCHMARKS = frozenset(benchmark_finders.GetContribBenchmarks())
ALL_SCHEDULEABLE_BENCHMARKS = OFFICIAL_BENCHMARKS | CONTRIB_BENCHMARKS
GTEST_STORY_NAME = '_gtest_'


def _IsPlatformSupported(benchmark, platform):
  supported = benchmark.GetSupportedPlatformNames(benchmark.SUPPORTED_PLATFORMS)
  return 'all' in supported or platform in supported


class PerfPlatform(object):
  def __init__(self,
               name,
               description,
               benchmark_configs,
               num_shards,
               platform_os,
               is_fyi=False,
               is_calibration=False,
               run_reference_build=False,
               pinpoint_only=False,
               executables=None):
    benchmark_configs = benchmark_configs.Frozenset()
    self._name = name
    self._description = description
    self._platform_os = platform_os
    # For sorting ignore case and "segments" in the bot name.
    self._sort_key = name.lower().replace('-', ' ')
    self._is_fyi = is_fyi
    self._is_calibration = is_calibration
    self.run_reference_build = run_reference_build
    self.pinpoint_only = pinpoint_only
    self.executables = executables or frozenset()
    assert num_shards
    self._num_shards = num_shards
    # pylint: disable=redefined-outer-name
    self._benchmark_configs = frozenset([
        b for b in benchmark_configs if
          _IsPlatformSupported(b.benchmark, self._platform_os)])
    # pylint: enable=redefined-outer-name
    benchmark_names = [config.name for config in self._benchmark_configs]
    assert len(set(benchmark_names)) == len(benchmark_names), (
        'Make sure that a benchmark does not appear twice.')

    base_file_name = name.replace(' ', '_').lower()
    self._timing_file_path = os.path.join(
        _SHARD_MAP_DIR, 'timing_data', base_file_name + '_timing.json')
    self.shards_map_file_name = base_file_name + '_map.json'
    self._shards_map_file_path = os.path.join(
        _SHARD_MAP_DIR, self.shards_map_file_name)

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
  def is_calibration(self):
    return self._is_calibration

  @property
  def is_official(self):
    return not self._is_fyi and not self.is_calibration

  @property
  def builder_url(self):
    return ('https://ci.chromium.org/p/chrome/builders/ci/%s' %
            six.moves.urllib.parse.quote(self._name))


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
    self._stories = None
    self.is_telemetry = True

  @property
  def name(self):
    return self.benchmark.Name()

  @property
  def repeat(self):
    return self.benchmark.options.get('pageset_repeat', 1)

  @property
  def stories(self):
    if self._stories is not None:
      return self._stories
    story_set = benchmark_utils.GetBenchmarkStorySet(self.benchmark())
    abridged_story_set_tag = (story_set.GetAbridgedStorySetTagFilter()
                              if self.abridged else None)
    story_filter_obj = story_filter.StoryFilter(
        abridged_story_set_tag=abridged_story_set_tag)
    stories = story_filter_obj.FilterStories(story_set)
    self._stories = [story.name for story in stories]
    return self._stories


class ExecutableConfig(object):
  def __init__(self, name, path=None, flags=None, estimated_runtime=60):
    self.name = name
    self.path = path or name
    self.flags = flags or []
    self.estimated_runtime = estimated_runtime
    self.abridged = False
    self.stories = [GTEST_STORY_NAME]
    self.is_telemetry = False
    self.repeat = 1


class PerfSuite(object):
  def __init__(self, configs):
    self._configs = dict()
    self.Add(configs)

  def Frozenset(self):
    return frozenset(self._configs.values())

  def Add(self, configs):
    if isinstance(configs, PerfSuite):
      configs = configs.Frozenset()
    for config in configs:
      if isinstance(config, str):
        config = _GetBenchmarkConfig(config)
      if config.name in self._configs:
        raise ValueError('Cannot have duplicate benchmarks/executables.')
      self._configs[config.name] = config
    return self

  def Remove(self, configs):
    for config in configs:
      name = config
      if isinstance(config, PerfSuite):
        name = config.name
      del self._configs[name]
    return self

  def Abridge(self, config_names):
    for name in config_names:
      del self._configs[name]
      self._configs[name] = _GetBenchmarkConfig(
          name, abridged=True)
    return self


# Global |benchmarks| is convenient way to keep BenchmarkConfig objects
# unique, which allows us to use set subtraction below.
benchmarks = {b.Name(): {True: BenchmarkConfig(b, abridged=True),
                         False: BenchmarkConfig(b, abridged=False)}
              for b in ALL_SCHEDULEABLE_BENCHMARKS}

def _GetBenchmarkConfig(benchmark_name, abridged=False):
  return benchmarks[benchmark_name][abridged]

OFFICIAL_BENCHMARK_CONFIGS = PerfSuite(
    [_GetBenchmarkConfig(b.Name()) for b in OFFICIAL_BENCHMARKS])
# power.mobile requires special hardware.
# only run blink_perf.sanitizer-api on linux-perf.
# speedometer2-chrome-health is only for use with the Chrome Health pipeline
OFFICIAL_BENCHMARK_CONFIGS = OFFICIAL_BENCHMARK_CONFIGS.Remove([
    'power.mobile',
    'blink_perf.sanitizer-api',
    'speedometer2-chrome-health',
    'speedometer2-minormc',
])
# TODO(crbug.com/965158): Remove OFFICIAL_BENCHMARK_NAMES once sharding
# scripts are no longer using it.
OFFICIAL_BENCHMARK_NAMES = frozenset(
    b.name for b in OFFICIAL_BENCHMARK_CONFIGS.Frozenset())

# TODO(crbug.com/1030840): Stop using these 'OFFICIAL_EXCEPT' suites and instead
# define each benchmarking config separately as is already done for many of the
# suites below.
_OFFICIAL_EXCEPT_DISPLAY_LOCKING = PerfSuite(OFFICIAL_BENCHMARK_CONFIGS).Remove(
    ['blink_perf.display_locking'])
_OFFICIAL_EXCEPT_JETSTREAM2 = PerfSuite(OFFICIAL_BENCHMARK_CONFIGS).Remove(
    ['jetstream2'])
_OFFICIAL_EXCEPT_DISPLAY_LOCKING_JETSTREAM2 = PerfSuite(
    OFFICIAL_BENCHMARK_CONFIGS).Remove(
        ['blink_perf.display_locking', 'jetstream2'])


def _sync_performance_tests(estimated_runtime=110,
                            path=None,
                            additional_flags=None):
  if not additional_flags:
    additional_flags = []
  flags = ['--test-launcher-jobs=1', '--test-launcher-retry-limit=0']
  flags.extend(additional_flags)
  return ExecutableConfig('sync_performance_tests',
                          path=path,
                          flags=flags,
                          estimated_runtime=estimated_runtime)


def _base_perftests(estimated_runtime=270, path=None, additional_flags=None):
  if not additional_flags:
    additional_flags = []
  flags = ['--test-launcher-jobs=1', '--test-launcher-retry-limit=0']
  flags.extend(additional_flags)
  return ExecutableConfig('base_perftests',
                          path=path,
                          flags=flags,
                          estimated_runtime=estimated_runtime)


def _components_perftests(estimated_runtime=110):
  return ExecutableConfig('components_perftests',
                          flags=[
                              '--xvfb',
                          ],
                          estimated_runtime=estimated_runtime)


def _dawn_perf_tests(estimated_runtime=270):
  return ExecutableConfig(
      'dawn_perf_tests',
      flags=['--test-launcher-jobs=1', '--test-launcher-retry-limit=0'],
      estimated_runtime=estimated_runtime)


def _load_library_perf_tests(estimated_runtime=3):
  return ExecutableConfig('load_library_perf_tests',
                          estimated_runtime=estimated_runtime)

def _performance_browser_tests(estimated_runtime=67):
  return ExecutableConfig(
      'performance_browser_tests',
      path='browser_tests',
      flags=[
          '--full-performance-run',
          '--test-launcher-jobs=1',
          '--test-launcher-retry-limit=0',
          # Allow the full performance runs to take up to 60 seconds (rather
          # than the default of 30 for normal CQ browser test runs).
          '--ui-test-action-timeout=60000',
          '--ui-test-action-max-timeout=60000',
          '--test-launcher-timeout=60000',
          '--gtest_filter=*/TabCapturePerformanceTest.*:'
          '*/CastV2PerformanceTest.*',
      ],
      estimated_runtime=estimated_runtime)


def _tracing_perftests(estimated_runtime=50):
  return ExecutableConfig('tracing_perftests',
                          estimated_runtime=estimated_runtime)


def _views_perftests(estimated_runtime=7):
  return ExecutableConfig('views_perftests',
                          flags=['--xvfb'],
                          estimated_runtime=estimated_runtime)

_CHROME_HEALTH_BENCHMARK_CONFIGS_DESKTOP = PerfSuite([
    _GetBenchmarkConfig('system_health.common_desktop')
])

FUCHSIA_EXEC_ARGS = {
    'astro': None,
    'sherlock': None,
    'atlas': None,
    'nelson': None,
    'nuc': None
}
FUCHSIA_EXEC_CONFIGS = {
    'astro': None,
    'sherlock': None,
    'atlas': None,
    'nelson': None,
    'nuc': None
}
_IMAGE_PATHS = {
    'astro': ('astro-release', 'smart_display_eng_arrested'),
    'sherlock': ('sherlock-release', 'smart_display_max_eng_arrested'),
    'nelson': ('nelson-release', 'smart_display_m3_eng_paused'),
}

# Some image paths are just a product-bundle, which is not a relative path.
_PB_IMAGE_PATHS = {
    'atlas': 'workstation_eng.chromebook-x64',
    'nuc': 'workstation_eng.x64',
}

_FUCHSIA_IMAGE_DIR = '../../third_party/fuchsia-sdk/images-internal/%s/%s'
_COMMON_FUCHSIA_ARGS = ['-d', '--os-check=update']
for board, path_parts in _IMAGE_PATHS.items():
  image_dir = _FUCHSIA_IMAGE_DIR % path_parts
  FUCHSIA_EXEC_ARGS[board] = _COMMON_FUCHSIA_ARGS + [
      '--system-image-dir=%s' % image_dir
  ]
  FUCHSIA_EXEC_CONFIGS[board] = frozenset([
      _base_perftests(900,
                      path='bin/run_base_perftests',
                      additional_flags=FUCHSIA_EXEC_ARGS[board])
  ])

for board, pb_name in _PB_IMAGE_PATHS.items():
  FUCHSIA_EXEC_ARGS[board] = _COMMON_FUCHSIA_ARGS + [
      f'--system-image-dir={pb_name}'
  ]
  FUCHSIA_EXEC_CONFIGS[board] = frozenset([
      _base_perftests(900,
                      path='bin/run_base_perftests',
                      additional_flags=FUCHSIA_EXEC_ARGS[board]),
      _sync_performance_tests(900,
                              path='bin/run_sync_performance_tests',
                              additional_flags=FUCHSIA_EXEC_ARGS[board]),
  ])

_LINUX_BENCHMARK_CONFIGS = PerfSuite(OFFICIAL_BENCHMARK_CONFIGS).Remove([
    'blink_perf.display_locking',
    'v8.runtime_stats.top_25',
]).Add([
    'blink_perf.sanitizer-api',
])
_LINUX_BENCHMARK_CONFIGS_WITH_MINORMC = PerfSuite(_LINUX_BENCHMARK_CONFIGS).Add(
    [
        'speedometer2-minormc',
    ])
_LINUX_EXECUTABLE_CONFIGS = frozenset([
    # TODO(crbug.com/811766): Add views_perftests.
    _base_perftests(200),
    _load_library_perf_tests(),
    _performance_browser_tests(165),
    _tracing_perftests(5),
])
_MAC_HIGH_END_BENCHMARK_CONFIGS = PerfSuite(OFFICIAL_BENCHMARK_CONFIGS).Remove([
    'blink_perf.display_locking',
    'v8.runtime_stats.top_25',
])
_MAC_HIGH_END_EXECUTABLE_CONFIGS = frozenset([
    _base_perftests(300),
    _dawn_perf_tests(330),
    _performance_browser_tests(190),
    _views_perftests(),
])
_MAC_LOW_END_BENCHMARK_CONFIGS = PerfSuite(OFFICIAL_BENCHMARK_CONFIGS).Remove([
    'jetstream2',
    'v8.runtime_stats.top_25',
])
_MAC_LOW_END_EXECUTABLE_CONFIGS = frozenset([
    _load_library_perf_tests(),
    _performance_browser_tests(210),
])
_MAC_M1_MINI_2020_BENCHMARK_CONFIGS = PerfSuite(
    OFFICIAL_BENCHMARK_CONFIGS).Remove([
        'blink_perf.display_locking',
        'v8.runtime_stats.top_25',
    ])
_MAC_M1_MINI_2020_PGO_BENCHMARK_CONFIGS = PerfSuite([
    _GetBenchmarkConfig('jetstream2'),
    _GetBenchmarkConfig('speedometer2'),
    _GetBenchmarkConfig('rendering.desktop'),
])
_MAC_M1_MINI_2020_EXECUTABLE_CONFIGS = frozenset([
    _base_perftests(300),
    _dawn_perf_tests(330),
    _performance_browser_tests(190),
    _views_perftests(),
])

_WIN_10_BENCHMARK_CONFIGS = PerfSuite(OFFICIAL_BENCHMARK_CONFIGS).Remove([
    'blink_perf.display_locking',
    'v8.runtime_stats.top_25',
])
_WIN_10_EXECUTABLE_CONFIGS = frozenset([
    _base_perftests(200),
    _components_perftests(125),
    _dawn_perf_tests(600),
    _views_perftests(),
])
_WIN_10_LOW_END_BENCHMARK_CONFIGS = PerfSuite(
    OFFICIAL_BENCHMARK_CONFIGS).Remove([
        'blink_perf.display_locking',
    ])
_WIN_10_LOW_END_HP_CANDIDATE_BENCHMARK_CONFIGS = PerfSuite([
    _GetBenchmarkConfig('v8.browsing_desktop'),
    _GetBenchmarkConfig('rendering.desktop', abridged=True),
])
_WIN_10_AMD_LAPTOP_BENCHMARK_CONFIGS = PerfSuite([
    _GetBenchmarkConfig('jetstream'),
    _GetBenchmarkConfig('jetstream2'),
    _GetBenchmarkConfig('kraken'),
    _GetBenchmarkConfig('octane'),
    _GetBenchmarkConfig('speedometer2'),
])
_WIN_7_BENCHMARK_CONFIGS = PerfSuite([
    'loading.desktop',
]).Abridge([
    'loading.desktop',
])
_WIN_7_GPU_BENCHMARK_CONFIGS = PerfSuite(['rendering.desktop']).Abridge(
    ['rendering.desktop'])
_ANDROID_GO_BENCHMARK_CONFIGS = PerfSuite([
    _GetBenchmarkConfig('system_health.memory_mobile'),
    _GetBenchmarkConfig('system_health.common_mobile'),
    _GetBenchmarkConfig('startup.mobile'),
    _GetBenchmarkConfig('system_health.webview_startup'),
    _GetBenchmarkConfig('v8.browsing_mobile'),
    _GetBenchmarkConfig('speedometer'),
    _GetBenchmarkConfig('speedometer2')])
_ANDROID_GO_WEBVIEW_BENCHMARK_CONFIGS = _ANDROID_GO_BENCHMARK_CONFIGS
_ANDROID_PIXEL2_BENCHMARK_CONFIGS = PerfSuite(_OFFICIAL_EXCEPT_DISPLAY_LOCKING)
_ANDROID_PIXEL2_EXECUTABLE_CONFIGS = frozenset([
    _components_perftests(60),
])
_ANDROID_PIXEL2_WEBVIEW_BENCHMARK_CONFIGS = PerfSuite(
    OFFICIAL_BENCHMARK_CONFIGS).Remove([
        'blink_perf.display_locking',
        'jetstream2',
        'v8.browsing_mobile-future',
    ])
_ANDROID_PIXEL4_BENCHMARK_CONFIGS = PerfSuite(_OFFICIAL_EXCEPT_DISPLAY_LOCKING)
_ANDROID_PIXEL4_EXECUTABLE_CONFIGS = frozenset([
    _components_perftests(60),
])
_ANDROID_PIXEL4_WEBVIEW_BENCHMARK_CONFIGS = PerfSuite(
    OFFICIAL_BENCHMARK_CONFIGS).Remove([
        'blink_perf.display_locking',
        'jetstream2',
        'v8.browsing_mobile-future',
    ])
_ANDROID_PIXEL4A_POWER_BENCHMARK_CONFIGS = PerfSuite([
    _GetBenchmarkConfig('power.mobile'),
    _GetBenchmarkConfig('system_health.scroll_jank_mobile')
])
_ANDROID_GO_WEMBLEY_BENCHMARK_CONFIGS = PerfSuite(
    [_GetBenchmarkConfig('startup.mobile'),
     _GetBenchmarkConfig('speedometer2')])
_ANDROID_NEXUS5X_FYI_BENCHMARK_CONFIGS = PerfSuite(
    [_GetBenchmarkConfig('system_health.scroll_jank_mobile')])
_ANDROID_PIXEL2_AAB_FYI_BENCHMARK_CONFIGS = PerfSuite(
    [_GetBenchmarkConfig('startup.mobile')])
_ANDROID_PIXEL2_FYI_BENCHMARK_CONFIGS = PerfSuite([
    _GetBenchmarkConfig('v8.browsing_mobile'),
    _GetBenchmarkConfig('system_health.memory_mobile'),
    _GetBenchmarkConfig('system_health.common_mobile'),
    _GetBenchmarkConfig('startup.mobile'),
    _GetBenchmarkConfig('speedometer2'),
    _GetBenchmarkConfig('rendering.mobile'),
    _GetBenchmarkConfig('octane'),
    _GetBenchmarkConfig('jetstream'),
    _GetBenchmarkConfig('system_health.scroll_jank_mobile')
])
_CHROMEOS_KEVIN_FYI_BENCHMARK_CONFIGS = PerfSuite([
    _GetBenchmarkConfig('rendering.desktop')])
_LACROS_BENCHMARK_CONFIGS = PerfSuite(OFFICIAL_BENCHMARK_CONFIGS).Remove([
    'blink_perf.display_locking',
    'v8.runtime_stats.top_25',
])
# Used for astro/nelson.
_FUCHSIA_PERF_ASTRO_BENCHMARK_CONFIGS = PerfSuite([
    _GetBenchmarkConfig('speedometer2'),
    _GetBenchmarkConfig('media.mobile'),
])
_FUCHSIA_PERF_SHERLOCK_BENCHMARK_CONFIGS = PerfSuite([
    _GetBenchmarkConfig('speedometer2'),
    _GetBenchmarkConfig('media.mobile'),
])
_LINUX_PERF_FYI_BENCHMARK_CONFIGS = PerfSuite([
    _GetBenchmarkConfig('power.desktop'),
    _GetBenchmarkConfig('rendering.desktop'),
    _GetBenchmarkConfig('system_health.common_desktop')
])
_FUCHSIA_PERF_FYI_BENCHMARK_CONFIGS = PerfSuite([
    _GetBenchmarkConfig('system_health.memory_desktop'),
    _GetBenchmarkConfig('rendering.mobile'),
    _GetBenchmarkConfig('jetstream2'),
])
_FUCHSIA_SHERLOCK_PERF_FYI_BENCHMARK_CONFIGS = PerfSuite([
    _GetBenchmarkConfig('system_health.memory_desktop'),
    _GetBenchmarkConfig('rendering.mobile'),
    _GetBenchmarkConfig('jetstream2'),
])
_FUCHSIA_ATLAS_PERF_FYI_BENCHMARK_CONFIGS = PerfSuite([
    _GetBenchmarkConfig('system_health.common_desktop'),
    _GetBenchmarkConfig('speedometer'),
    _GetBenchmarkConfig('speedometer2'),
    _GetBenchmarkConfig('jetstream'),
    _GetBenchmarkConfig('jetstream2'),
])
_FUCHSIA_NUC_PERF_FYI_BENCHMARK_CONFIGS = PerfSuite([
    _GetBenchmarkConfig('system_health.common_desktop'),
    _GetBenchmarkConfig('speedometer'),
    _GetBenchmarkConfig('speedometer2'),
    _GetBenchmarkConfig('jetstream'),
    _GetBenchmarkConfig('jetstream2'),
])
_LINUX_PERF_CALIBRATION_BENCHMARK_CONFIGS = PerfSuite([
    _GetBenchmarkConfig('speedometer2'),
    _GetBenchmarkConfig('blink_perf.shadow_dom'),
    _GetBenchmarkConfig('system_health.common_desktop'),
])
_ANDROID_PIXEL2_PERF_CALIBRATION_BENCHMARK_CONFIGS = PerfSuite([
    _GetBenchmarkConfig('system_health.common_mobile'),
    _GetBenchmarkConfig('system_health.memory_mobile'),
])


# Linux
LINUX = PerfPlatform('linux-perf',
                     'Ubuntu-18.04, 8 core, NVIDIA Quadro P400',
                     _LINUX_BENCHMARK_CONFIGS_WITH_MINORMC,
                     26,
                     'linux',
                     executables=_LINUX_EXECUTABLE_CONFIGS)
LINUX_PGO = PerfPlatform('linux-perf-pgo',
                         'Ubuntu-18.04, 8 core, NVIDIA Quadro P400',
                         _LINUX_BENCHMARK_CONFIGS,
                         26,
                         'linux',
                         executables=_LINUX_EXECUTABLE_CONFIGS)
LINUX_REL = PerfPlatform(
    'linux-perf-rel',
    'Ubuntu-18.04, 8 core, NVIDIA Quadro P400',
    _CHROME_HEALTH_BENCHMARK_CONFIGS_DESKTOP,
    2,
    'linux',
    executables=_LINUX_EXECUTABLE_CONFIGS)

# Mac
MAC_HIGH_END_LAPTOP = PerfPlatform(
    'mac-laptop_high_end-perf',
    'MacBook Pro, Core i7 2.8 GHz, 16GB RAM, 256GB SSD, Radeon 55',
    _MAC_HIGH_END_BENCHMARK_CONFIGS,
    26,
    'mac',
    executables=_MAC_HIGH_END_EXECUTABLE_CONFIGS)
MAC_HIGH_END_LAPTOP_PGO = PerfPlatform(
    'mac-laptop_high_end-perf-pgo',
    'MacBook Pro, Core i7 2.8 GHz, 16GB RAM, 256GB SSD, Radeon 55',
    _MAC_HIGH_END_BENCHMARK_CONFIGS,
    26,
    'mac',
    executables=_MAC_HIGH_END_EXECUTABLE_CONFIGS)
MAC_LOW_END_LAPTOP = PerfPlatform(
    'mac-laptop_low_end-perf',
    'MacBook Air, Core i5 1.8 GHz, 8GB RAM, 128GB SSD, HD Graphics',
    _MAC_LOW_END_BENCHMARK_CONFIGS,
    26,
    'mac',
    executables=_MAC_LOW_END_EXECUTABLE_CONFIGS)
MAC_LOW_END_LAPTOP_PGO = PerfPlatform(
    'mac-laptop_low_end-perf-pgo',
    'MacBook Air, Core i5 1.8 GHz, 8GB RAM, 128GB SSD, HD Graphics',
    _MAC_LOW_END_BENCHMARK_CONFIGS,
    26,
    'mac',
    executables=_MAC_LOW_END_EXECUTABLE_CONFIGS)
MAC_M1_MINI_2020 = PerfPlatform(
    'mac-m1_mini_2020-perf',
    'Mac M1 Mini 2020',
    _MAC_M1_MINI_2020_BENCHMARK_CONFIGS,
    26,
    'mac',
    executables=_MAC_M1_MINI_2020_EXECUTABLE_CONFIGS)
MAC_M1_MINI_2020_PGO = PerfPlatform(
    'mac-m1_mini_2020-perf-pgo',
    'Mac M1 Mini 2020',
    _MAC_M1_MINI_2020_PGO_BENCHMARK_CONFIGS,
    4,
    'mac')

# Win
WIN_10_LOW_END = PerfPlatform(
    'win-10_laptop_low_end-perf',
    'Low end windows 10 HP laptops. HD Graphics 5500, x86-64-i3-5005U, '
    'SSD, 4GB RAM.',
    _WIN_10_LOW_END_BENCHMARK_CONFIGS,
    # TODO(crbug.com/1305291): Increase the count back to 46 when issue fixed.
    40,
    'win')
WIN_10_LOW_END_PGO = PerfPlatform(
    'win-10_laptop_low_end-perf-pgo',
    'Low end windows 10 HP laptops. HD Graphics 5500, x86-64-i3-5005U, '
    'SSD, 4GB RAM.',
    _WIN_10_LOW_END_BENCHMARK_CONFIGS,
    # TODO(crbug.com/1305291): Increase the count back to 46 when issue fixed.
    40,
    'win')
WIN_10 = PerfPlatform(
    'win-10-perf',
    'Windows Intel HD 630 towers, Core i7-7700 3.6 GHz, 16GB RAM,'
    ' Intel Kaby Lake HD Graphics 630',
    _WIN_10_BENCHMARK_CONFIGS,
    20,
    'win',
    executables=_WIN_10_EXECUTABLE_CONFIGS)
WIN_10_PGO = PerfPlatform(
    'win-10-perf-pgo',
    'Windows Intel HD 630 towers, Core i7-7700 3.6 GHz, 16GB RAM,'
    ' Intel Kaby Lake HD Graphics 630',
    _WIN_10_BENCHMARK_CONFIGS,
    26,
    'win',
    executables=_WIN_10_EXECUTABLE_CONFIGS)
WIN_10_AMD_LAPTOP = PerfPlatform('win-10_amd_laptop-perf',
                                 'Windows 10 Laptop with AMD chipset.',
                                 _WIN_10_AMD_LAPTOP_BENCHMARK_CONFIGS, 5, 'win')
WIN_10_AMD_LAPTOP_PGO = PerfPlatform('win-10_amd_laptop-perf-pgo',
                                     'Windows 10 Laptop with AMD chipset.',
                                     _WIN_10_AMD_LAPTOP_BENCHMARK_CONFIGS, 5,
                                     'win')

# Android
ANDROID_GO = PerfPlatform('android-go-perf', 'Android O (gobo)',
                          _ANDROID_GO_BENCHMARK_CONFIGS, 13, 'android')
ANDROID_GO_PGO = PerfPlatform('android-go-perf-pgo', 'Android O (gobo)',
                              _ANDROID_GO_BENCHMARK_CONFIGS, 13, 'android')
ANDROID_GO_WEBVIEW = PerfPlatform('android-go_webview-perf',
                                  'Android OPM1.171019.021 (gobo)',
                                  _ANDROID_GO_WEBVIEW_BENCHMARK_CONFIGS, 10,
                                  'android')
ANDROID_PIXEL2 = PerfPlatform('android-pixel2-perf',
                              'Android OPM1.171019.021',
                              _ANDROID_PIXEL2_BENCHMARK_CONFIGS,
                              28,
                              'android',
                              executables=_ANDROID_PIXEL2_EXECUTABLE_CONFIGS)
ANDROID_PIXEL2_PGO = PerfPlatform(
    'android-pixel2-perf-pgo',
    'Android OPM1.171019.021',
    _ANDROID_PIXEL2_BENCHMARK_CONFIGS,
    28,
    'android',
    executables=_ANDROID_PIXEL2_EXECUTABLE_CONFIGS)
ANDROID_PIXEL2_WEBVIEW = PerfPlatform(
    'android-pixel2_webview-perf', 'Android OPM1.171019.021',
    _ANDROID_PIXEL2_WEBVIEW_BENCHMARK_CONFIGS, 21, 'android')
ANDROID_PIXEL2_WEBVIEW_PGO = PerfPlatform(
    'android-pixel2_webview-perf-pgo', 'Android OPM1.171019.021',
    _ANDROID_PIXEL2_WEBVIEW_BENCHMARK_CONFIGS, 21, 'android')
ANDROID_PIXEL4 = PerfPlatform('android-pixel4-perf',
                              'Android R',
                              _ANDROID_PIXEL4_BENCHMARK_CONFIGS,
                              28,
                              'android',
                              executables=_ANDROID_PIXEL4_EXECUTABLE_CONFIGS)
ANDROID_PIXEL4_PGO = PerfPlatform(
    'android-pixel4-perf-pgo',
    'Android R',
    _ANDROID_PIXEL4_BENCHMARK_CONFIGS,
    28,
    'android',
    executables=_ANDROID_PIXEL4_EXECUTABLE_CONFIGS)
ANDROID_PIXEL4_WEBVIEW = PerfPlatform(
    'android-pixel4_webview-perf', 'Android R',
    _ANDROID_PIXEL4_WEBVIEW_BENCHMARK_CONFIGS, 21, 'android')
ANDROID_PIXEL4A_POWER = PerfPlatform('android-pixel4a_power-perf',
                                     'Android QD4A.200102.001.A1',
                                     _ANDROID_PIXEL4A_POWER_BENCHMARK_CONFIGS,
                                     12, 'android')
ANDROID_PIXEL4A_POWER_PGO = PerfPlatform(
    'android-pixel4a_power-perf-pgo', 'Android QD4A.200102.001.A1',
    _ANDROID_PIXEL4A_POWER_BENCHMARK_CONFIGS, 12, 'android')
ANDROID_GO_WEMBLEY = PerfPlatform('android-go-wembley-perf',
                                  'Android U',
                                  _ANDROID_GO_WEMBLEY_BENCHMARK_CONFIGS, 2,
                                  'android')
ANDROID_NEW_PIXEL = PerfPlatform('android-new-pixel-perf',
                                 'Android T',
                                 PerfSuite([]),
                                 1,
                                 'android',
                                 pinpoint_only=True)
ANDROID_NEW_PIXEL_PGO = PerfPlatform('android-new-pixel-perf-pgo',
                                     'Android T',
                                     PerfSuite([]),
                                     1,
                                     'android',
                                     pinpoint_only=True)
ANDROID_NEW_PIXEL_PRO = PerfPlatform('android-new-pixel-pro-perf',
                                     'Android T',
                                     PerfSuite([]),
                                     1,
                                     'android',
                                     pinpoint_only=True)
ANDROID_NEW_PIXEL_PRO_PGO = PerfPlatform('android-new-pixel-pro-perf-pgo',
                                         'Android T',
                                         PerfSuite([]),
                                         1,
                                         'android',
                                         pinpoint_only=True)

# Cros/Lacros
LACROS_EVE_PERF = PerfPlatform('lacros-eve-perf', '', _LACROS_BENCHMARK_CONFIGS,
                               4, 'chromeos')
LACROS_X86_PERF = PerfPlatform('lacros-x86-perf', '', _LACROS_BENCHMARK_CONFIGS,
                               6, 'chromeos')
# Fuchsia
FUCHSIA_PERF_ASTRO = PerfPlatform('fuchsia-perf-ast',
                                  '',
                                  _FUCHSIA_PERF_ASTRO_BENCHMARK_CONFIGS,
                                  2,
                                  'fuchsia',
                                  is_fyi=True,
                                  executables=FUCHSIA_EXEC_CONFIGS['astro'])
FUCHSIA_PERF_NELSON = PerfPlatform('fuchsia-perf-nsn',
                                   '',
                                   _FUCHSIA_PERF_ASTRO_BENCHMARK_CONFIGS,
                                   2,
                                   'fuchsia',
                                   is_fyi=True,
                                   executables=FUCHSIA_EXEC_CONFIGS['nelson'])
FUCHSIA_PERF_SHERLOCK = PerfPlatform(
    'fuchsia-perf-shk',
    '',
    _FUCHSIA_PERF_SHERLOCK_BENCHMARK_CONFIGS,
    2,
    'fuchsia',
    is_fyi=True,
    executables=FUCHSIA_EXEC_CONFIGS['sherlock'])

# FYI bots
WIN_10_LOW_END_HP_CANDIDATE = PerfPlatform(
    'win-10_laptop_low_end-perf_HP-Candidate', 'HP 15-BS121NR Laptop Candidate',
    _WIN_10_LOW_END_HP_CANDIDATE_BENCHMARK_CONFIGS,
    1, 'win', is_fyi=True)
ANDROID_PIXEL2_PERF_AAB_FYI = PerfPlatform(
    'android-pixel2-perf-aab-fyi',
    'Android OPM1.171019.021',
    _ANDROID_PIXEL2_AAB_FYI_BENCHMARK_CONFIGS,
    1,
    'android',
    is_fyi=True)
ANDROID_PIXEL2_PERF_FYI = PerfPlatform('android-pixel2-perf-fyi',
                                       'Android OPM1.171019.021',
                                       _ANDROID_PIXEL2_FYI_BENCHMARK_CONFIGS,
                                       4,
                                       'android',
                                       is_fyi=True)
CHROMEOS_KEVIN_PERF_FYI = PerfPlatform('chromeos-kevin-perf-fyi',
                                       '',
                                       _CHROMEOS_KEVIN_FYI_BENCHMARK_CONFIGS,
                                       4,
                                       'chromeos',
                                       is_fyi=True)
LINUX_PERF_FYI = PerfPlatform('linux-perf-fyi',
                              '',
                              _LINUX_PERF_FYI_BENCHMARK_CONFIGS,
                              1,
                              'linux',
                              is_fyi=True)
# TODO(crbug.com/1268204): Rename to platform-specific name.
FUCHSIA_PERF_FYI = PerfPlatform('fuchsia-perf-fyi',
                                '',
                                _FUCHSIA_PERF_FYI_BENCHMARK_CONFIGS,
                                10,
                                'fuchsia',
                                is_fyi=True)
FUCHSIA_PERF_SHERLOCK_FYI = PerfPlatform(
    'fuchsia-perf-sherlock-fyi',
    '',
    _FUCHSIA_SHERLOCK_PERF_FYI_BENCHMARK_CONFIGS,
    6,
    'fuchsia',
    is_fyi=True)
FUCHSIA_PERF_ATLAS_FYI = PerfPlatform('fuchsia-perf-atlas-fyi',
                                      '',
                                      _FUCHSIA_ATLAS_PERF_FYI_BENCHMARK_CONFIGS,
                                      4,
                                      'fuchsia',
                                      is_fyi=True,
                                      executables=FUCHSIA_EXEC_CONFIGS['atlas'])
FUCHSIA_PERF_NUC_FYI = PerfPlatform('fuchsia-perf-nuc-fyi',
                                    '',
                                    _FUCHSIA_NUC_PERF_FYI_BENCHMARK_CONFIGS,
                                    4,
                                    'fuchsia',
                                    is_fyi=True,
                                    executables=FUCHSIA_EXEC_CONFIGS['nuc'])

# Calibration bots
LINUX_PERF_CALIBRATION = PerfPlatform(
    'linux-perf-calibration',
    'Ubuntu-18.04, 8 core, NVIDIA Quadro P400',
    _LINUX_BENCHMARK_CONFIGS,
    28,
    'linux',
    executables=_LINUX_EXECUTABLE_CONFIGS,
    is_calibration=True)

ANDROID_PIXEL2_PERF_CALIBRATION = PerfPlatform(
    'android-pixel2-perf-calibration',
    'Android OPM1.171019.021',
    _ANDROID_PIXEL2_BENCHMARK_CONFIGS,
    42,
    'android',
    executables=_ANDROID_PIXEL2_EXECUTABLE_CONFIGS,
    is_calibration=True)

ALL_PLATFORMS = {
    p for p in locals().values() if isinstance(p, PerfPlatform)
}
PLATFORMS_BY_NAME = {p.name: p for p in ALL_PLATFORMS}
FYI_PLATFORMS = {
    p for p in ALL_PLATFORMS if p.is_fyi
}
CALIBRATION_PLATFORMS = {p for p in ALL_PLATFORMS if p.is_calibration}
OFFICIAL_PLATFORMS = {p for p in ALL_PLATFORMS if p.is_official}
ALL_PLATFORM_NAMES = {
    p.name for p in ALL_PLATFORMS
}
OFFICIAL_PLATFORM_NAMES = {
    p.name for p in OFFICIAL_PLATFORMS
}


def find_bot_platform(builder_name):
  for bot_platform in ALL_PLATFORMS:
    if bot_platform.name == builder_name:
      return bot_platform
  return None
