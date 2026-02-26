# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

# pylint: disable=too-many-lines

import csv
import os
import pathlib
import io
import shlex


from typing import Callable, Final, Iterable, Optional, Union

import six.moves.urllib.parse  # pylint: disable=import-error

from core import benchmark_finders
from core import benchmark_utils

from telemetry.story import story_filter


_SHARD_MAP_DIR = os.path.join(os.path.dirname(__file__), 'shard_maps')

_ALL_BENCHMARKS_BY_NAMES = dict(
    (b.Name(), b) for b in benchmark_finders.GetAllBenchmarks())

_OFFICIAL_BENCHMARKS = frozenset(
    b for b in benchmark_finders.GetOfficialBenchmarks() if b.IsScheduled())
_CONTRIB_BENCHMARKS = frozenset(benchmark_finders.GetContribBenchmarks())
_ALL_SCHEDULEABLE_BENCHMARKS = _OFFICIAL_BENCHMARKS | _CONTRIB_BENCHMARKS
GTEST_STORY_NAME = '_gtest_'


def _IsPlatformSupported(benchmark, platform: str) -> bool:
  supported = benchmark.GetSupportedPlatformNames(benchmark.SUPPORTED_PLATFORMS)
  return 'all' in supported or platform in supported


class _PerfPlatform(object):
  def __init__(self,
               name: str,
               description: str,
               benchmark_configs: PerfSuite,
               num_shards: int,
               platform_os: str,
               is_fyi: bool = False,
               run_reference_build: bool = False,
               pinpoint_only: bool = False,
               executables: Optional[frozenset[ExecutableConfig]] = None,
               crossbench: Optional[frozenset[CrossbenchConfig]] = None):
    benchmark_config_set = benchmark_configs.Frozenset()
    self._name = name
    self._description = description
    self._platform_os = platform_os
    # For sorting ignore case and 'segments' in the bot name.
    self._sort_key = name.lower().replace('-', ' ')
    self._is_fyi = is_fyi
    self.run_reference_build = run_reference_build
    self.pinpoint_only = pinpoint_only
    self.executables = executables or frozenset()
    self.crossbench = crossbench or frozenset()
    assert num_shards
    self._num_shards = num_shards
    # pylint: disable=redefined-outer-name
    self._benchmark_configs: frozenset[TelemetryConfig] = frozenset([
        b for b in benchmark_config_set
        if _IsPlatformSupported(b.benchmark, self._platform_os)
    ])
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
  def num_shards(self) -> int:
    return self._num_shards

  @property
  def shards_map_file_path(self) -> str:
    return self._shards_map_file_path

  @property
  def timing_file_path(self) -> str:
    return self._timing_file_path

  @property
  def name(self) -> str:
    return self._name

  @property
  def description(self) -> str:
    return self._description

  @property
  def platform(self) -> str:
    return self._platform_os

  @property
  def benchmarks_to_run(self) -> frozenset[TelemetryConfig]:
    # TODO(crbug.com/40628256): Deprecate this in favor of benchmark_configs
    # as part of change to make sharding scripts accommodate abridged
    # benchmarks.
    return frozenset({b.benchmark for b in self._benchmark_configs})

  @property
  def benchmark_configs(self) -> frozenset[TelemetryConfig]:
    return self._benchmark_configs

  @property
  def is_fyi(self) -> bool:
    return self._is_fyi

  @property
  def is_official(self) -> bool:
    return not self._is_fyi

  @property
  def builder_url(self) -> Optional[str]:
    if self.pinpoint_only:
      return None
    return ('https://ci.chromium.org/p/chrome/builders/ci/%s' %
            six.moves.urllib.parse.quote(self._name))


class BenchmarkConfig(object):

  def __init__(self, name: str) -> None:
    self.name: Final[str] = name


class TelemetryConfig(BenchmarkConfig):

  def __init__(self,
               benchmark,
               abridged: bool = False,
               pageset_repeat_override: Optional[int] = None):
    """A configuration for a benchmark that helps decide how to shard it.

    Args:
      benchmark: the benchmark.Benchmark object.
      abridged: True if the benchmark should be abridged so fewer stories
        are run, and False if the whole benchmark should be run.
      pageset_repeat_override: number of times to repeat the entire story set.
        can be None, which defaults to the benchmark default pageset_repeat.
    """
    super().__init__(benchmark.Name())
    self.benchmark = benchmark
    self.abridged = abridged
    self._stories = None
    self._exhaustive_stories = None
    self.is_telemetry = True
    self.pageset_repeat_override = pageset_repeat_override

  @property
  def repeat(self) -> int:
    if self.pageset_repeat_override is not None:
      return self.pageset_repeat_override
    return self.benchmark.options.get('pageset_repeat', 1)

  @property
  def extra_flags(self):
    return ()

  @property
  def stories(self) -> list[str]:
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

  @property
  def exhaustive_stories(self) -> list[str]:
    if self._exhaustive_stories is not None:
      return self._exhaustive_stories
    story_set = benchmark_utils.GetBenchmarkStorySet(self.benchmark(),
                                                     exhaustive=True)
    abridged_story_set_tag = (story_set.GetAbridgedStorySetTagFilter()
                              if self.abridged else None)
    story_filter_obj = story_filter.StoryFilter(
        abridged_story_set_tag=abridged_story_set_tag)
    stories = story_filter_obj.FilterStories(story_set)
    self._exhaustive_stories = [story.name for story in stories]
    return self._exhaustive_stories


class ExecutableConfig(BenchmarkConfig):

  def __init__(self,
               name: str,
               path: Optional[str] = None,
               flags: tuple[str, ...] = (),
               estimated_runtime: int = 60,
               extra_flags: tuple[str, ...] = ()):
    super().__init__(name)
    self.path = path or name
    self.flags = flags or ()
    self.extra_flags = extra_flags or ()
    self.estimated_runtime = estimated_runtime
    self.abridged = False
    self.stories = [GTEST_STORY_NAME]
    self.is_telemetry = False
    self.repeat = 1


class CrossbenchConfig(BenchmarkConfig):
  def __init__(self,
               name: str,
               crossbench_name: str,
               estimated_runtime: int = 60,
               stories=None,
               flags: tuple[str, ...] = ()):
    super().__init__(name)
    self.crossbench_name = crossbench_name
    self.estimated_runtime = estimated_runtime
    self.stories = stories or ['default']
    self.arguments: tuple[str, ...] = flags
    self.repeat = 1

  @property
  def extra_flags(self) -> tuple[str, ...]:
    return self.arguments


class PerfSuite(object):
  def __init__(self, configs):
    self._configs: dict[str, TelemetryConfig] = dict()
    self.Add(configs)

  def Frozenset(self) -> frozenset[TelemetryConfig]:
    return frozenset(self._configs.values())

  def Add(
      self, configs: Union[Iterable[Union[str, TelemetryConfig]], PerfSuite]
  ) -> PerfSuite:
    if isinstance(configs, PerfSuite):
      configs = configs.Frozenset()
    for config in configs:
      if isinstance(config, str):
        config = _TelemetryConfig(config)
      if config.name in self._configs:
        raise ValueError('Cannot have duplicate benchmarks/executables.')
      self._configs[config.name] = config
    return self

  def Remove(self, config_names: Iterable[str]) -> PerfSuite:
    for name in config_names:
      del self._configs[name]
    return self

  def Abridge(self, config_names: Iterable[str]) -> PerfSuite:
    for name in config_names:
      del self._configs[name]
      self._configs[name] = _TelemetryConfig(name, abridged=True)
    return self

  def Repeat(self, config_names: Iterable[str],
             pageset_repeat: int) -> PerfSuite:
    for name in config_names:
      self._configs[name] = _TelemetryConfig(
          name,
          abridged=self._configs[name].abridged,
          pageset_repeat=pageset_repeat)
    return self


def _TelemetryConfig(benchmark_name: str,
                     abridged: bool = False,
                     pageset_repeat: int | None = None):
  benchmark = _ALL_BENCHMARKS_BY_NAMES[benchmark_name]
  return TelemetryConfig(benchmark, abridged, pageset_repeat)


_OFFICIAL_BENCHMARK_CONFIGS = PerfSuite(
    [_TelemetryConfig(b.Name()) for b in _OFFICIAL_BENCHMARKS])
_OFFICIAL_BENCHMARK_CONFIGS = _OFFICIAL_BENCHMARK_CONFIGS.Remove([
    'blink_perf.svg',
    'blink_perf.paint',
])
# TODO(crbug.com/40628256): Remove OFFICIAL_BENCHMARK_NAMES once sharding
# scripts are no longer using it.
OFFICIAL_BENCHMARK_NAMES = frozenset(
    b.name for b in _OFFICIAL_BENCHMARK_CONFIGS.Frozenset())

BenchmarkConfigFactory = Callable[..., BenchmarkConfig]
_BENCHMARKS_CONFIG_FACTORIES: dict[str, BenchmarkConfigFactory] = {}
for b in _ALL_BENCHMARKS_BY_NAMES:
  _BENCHMARKS_CONFIG_FACTORIES[b] = _TelemetryConfig


def _register(name):

  def _decorator(func):
    if name in _BENCHMARKS_CONFIG_FACTORIES:
      raise ValueError('Duplicate benchmark config: %s' % name)
    _BENCHMARKS_CONFIG_FACTORIES[name] = func
    return func

  return _decorator


@_register('sync_performance_tests')
def _sync_performance_tests(estimated_runtime: int = 110,
                            path=None,
                            extra_flags: tuple[str, ...] = ()):
  flags = ('--test-launcher-jobs=1', '--test-launcher-retry-limit=0')
  flags += extra_flags
  return ExecutableConfig('sync_performance_tests',
                          path=path,
                          flags=flags,
                          extra_flags=extra_flags,
                          estimated_runtime=estimated_runtime)


@_register('base_perftests')
def _base_perftests(estimated_runtime: int = 270,
                    path=None,
                    extra_flags: tuple[str, ...] = ()):
  flags = ('--test-launcher-jobs=1', '--test-launcher-retry-limit=0')
  flags += extra_flags
  return ExecutableConfig('base_perftests',
                          path=path,
                          flags=flags,
                          extra_flags=extra_flags,
                          estimated_runtime=estimated_runtime)


@_register('components_perftests')
def _components_perftests(estimated_runtime: int = 110):
  return ExecutableConfig('components_perftests',
                          flags=('--xvfb', ),
                          estimated_runtime=estimated_runtime)


@_register('dawn_perf_tests')
def _dawn_perf_tests(estimated_runtime: int = 270):
  return ExecutableConfig('dawn_perf_tests',
                          flags=('--test-launcher-jobs=1',
                                 '--test-launcher-retry-limit=0'),
                          estimated_runtime=estimated_runtime)


@_register('tint_benchmark')
def _tint_benchmark(estimated_runtime: int = 180):
  return ExecutableConfig('tint_benchmark',
                          flags=('--use-chrome-perf-format', ),
                          estimated_runtime=estimated_runtime)


@_register('load_library_perf_tests')
def _load_library_perf_tests(estimated_runtime: int = 3):
  return ExecutableConfig('load_library_perf_tests',
                          estimated_runtime=estimated_runtime)


@_register('performance_browser_tests')
def _performance_browser_tests(estimated_runtime: int = 67):
  return ExecutableConfig(
      'performance_browser_tests',
      path='browser_tests',
      flags=(
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
      ),
      estimated_runtime=estimated_runtime)


@_register('tracing_perftests')
def _tracing_perftests(estimated_runtime: int = 5):
  return ExecutableConfig('tracing_perftests',
                          estimated_runtime=estimated_runtime)


@_register('views_perftests')
def _views_perftests(estimated_runtime: int = 7):
  return ExecutableConfig('views_perftests',
                          flags=('--xvfb', ),
                          estimated_runtime=estimated_runtime)


@_register('web_tests_cuj')
def _web_tests_cuj(estimated_runtime: int = 10):
  return ExecutableConfig('web_tests_cuj',
                          path='../../tools/perf/web_tests_cuj.py',
                          estimated_runtime=estimated_runtime)

# Speedometer:
@_register('speedometer2.0.crossbench')
def _speedometer2_0_crossbench(estimated_runtime: int = 60,
                               flags: tuple[str, ...] = ()):
  return CrossbenchConfig('speedometer2.0.crossbench',
                          'speedometer_2.0',
                          estimated_runtime=estimated_runtime,
                          flags=flags)


@_register('speedometer2.1.crossbench')
def _speedometer2_1_crossbench(estimated_runtime: int = 60,
                               flags: tuple[str, ...] = ()):
  return CrossbenchConfig('speedometer2.1.crossbench',
                          'speedometer_2.1',
                          estimated_runtime=estimated_runtime,
                          flags=flags)


@_register('speedometer2.crossbench')
def _speedometer2_crossbench(estimated_runtime: int = 60,
                             flags: tuple[str, ...] = ()):
  """Alias for the latest Speedometer 2.X version."""
  return CrossbenchConfig('speedometer2.crossbench',
                          'speedometer_2',
                          estimated_runtime=estimated_runtime,
                          flags=flags)


@_register('speedometer3.0.crossbench')
def _speedometer3_0_crossbench(estimated_runtime: int = 60,
                               flags: tuple[str, ...] = ()):
  return CrossbenchConfig('speedometer3.0.crossbench',
                          'speedometer_3.0',
                          estimated_runtime=estimated_runtime,
                          flags=flags)


@_register('speedometer3.1.crossbench')
def _speedometer3_1_crossbench(estimated_runtime: int = 60,
                               flags: tuple[str, ...] = ()):
  return CrossbenchConfig('speedometer3.1.crossbench',
                          'speedometer_3.1',
                          estimated_runtime=estimated_runtime,
                          flags=flags)


@_register('speedometer3.crossbench')
def _speedometer3_crossbench(estimated_runtime: int = 60,
                             flags: tuple[str, ...] = ()):
  """Alias for the latest Speedometer 3.X version."""
  return CrossbenchConfig('speedometer3.crossbench',
                          'speedometer_3',
                          estimated_runtime=estimated_runtime,
                          flags=flags)


@_register('speedometer_main.crossbench')
def _speedometer_main_crossbench(estimated_runtime: int = 60,
                                 flags: tuple[str, ...] = ()):
  # The latest WIP speedometer version
  flags += ('--detailed-metrics', )
  return CrossbenchConfig('speedometer_main.crossbench',
                          'speedometer_main',
                          estimated_runtime=estimated_runtime,
                          flags=flags)


@_register('speedometer3.a11y.crossbench')
def _speedometer3_a11y_crossbench(estimated_runtime: int = 60,
                                  flags: tuple[str, ...] = ()):
  """Latest Speedometer 3 with accessibility flag enabled."""
  flags += ('--extra-browser-args=--force-renderer-accessibility', )
  return CrossbenchConfig('speedometer3.a11y.crossbench',
                          'speedometer_3',
                          estimated_runtime=estimated_runtime,
                          flags=flags)


# MotionMark:
@_register('motionmark1.2.crossbench')
def _motionmark1_2_crossbench(estimated_runtime: int = 360,
                              flags: tuple[str, ...] = ()):
  return CrossbenchConfig('motionmark1.2.crossbench',
                          'motionmark_1.2',
                          estimated_runtime=estimated_runtime,
                          flags=flags)


@_register('motionmark1.3.0.crossbench')
def _motionmark1_3_0_crossbench(estimated_runtime: int = 360,
                                flags: tuple[str, ...] = ()):
  return CrossbenchConfig('motionmark1.3.0.crossbench',
                          'motionmark_1.3.0',
                          estimated_runtime=estimated_runtime,
                          flags=flags)


@_register('motionmark1.3.1.crossbench')
def _motionmark1_3_1_crossbench(estimated_runtime: int = 360,
                                flags: tuple[str, ...] = ()):
  return CrossbenchConfig('motionmark1.3.1.crossbench',
                          'motionmark_1.3.1',
                          estimated_runtime=estimated_runtime,
                          flags=flags)


@_register('motionmark1.3.crossbench')
def _motionmark1_3_crossbench(estimated_runtime: int = 360,
                              flags: tuple[str, ...] = ()):
  """Alias for the latest MotionMark 1.3.X version."""
  return CrossbenchConfig('motionmark1.3.crossbench',
                          'motionmark_1.3',
                          estimated_runtime=estimated_runtime,
                          flags=flags)


@_register('motionmark_main.crossbench')
def _motionmark_main_crossbench(estimated_runtime: int = 360,
                                flags: tuple[str, ...] = ()):
  return CrossbenchConfig('motionmark_main.crossbench',
                          'motionmark_main',
                          estimated_runtime=estimated_runtime,
                          flags=flags)


# JetStream:
@_register('jetstream2.0.crossbench')
def _jetstream2_0_crossbench(estimated_runtime: int = 180,
                             flags: tuple[str, ...] = ()):
  return CrossbenchConfig('jetstream2.0.crossbench',
                          'jetstream_2.0',
                          estimated_runtime=estimated_runtime,
                          flags=flags)


@_register('jetstream2.1.crossbench')
def _jetstream2_1_crossbench(estimated_runtime: int = 180,
                             flags: tuple[str, ...] = ()):
  return CrossbenchConfig('jetstream2.1.crossbench',
                          'jetstream_2.1',
                          estimated_runtime=estimated_runtime,
                          flags=flags)


@_register('jetstream2.2.crossbench')
def _jetstream2_2_crossbench(estimated_runtime: int = 180,
                             flags: tuple[str, ...] = ()):
  return CrossbenchConfig('jetstream2.2.crossbench',
                          'jetstream_2.2',
                          estimated_runtime=estimated_runtime,
                          flags=flags)


@_register('jetstream2.crossbench')
def _jetstream2_crossbench(estimated_runtime: int = 180,
                           flags: tuple[str, ...] = ()):
  """Alias of the latest JetStream 2.X version."""
  return CrossbenchConfig('jetstream2.crossbench',
                          'jetstream_2',
                          estimated_runtime=estimated_runtime,
                          flags=flags)


@_register('jetstream3.0.crossbench')
def _jetstream3_0_crossbench(estimated_runtime: int = 180,
                             flags: tuple[str, ...] = ()):
  return CrossbenchConfig('jetstream3_0.crossbench',
                          'jetstream_3.0',
                          estimated_runtime=estimated_runtime,
                          flags=flags)


@_register('jetstream3.crossbench')
def _jetstream3_crossbench(estimated_runtime: int = 180,
                           flags: tuple[str, ...] = ()):
  return CrossbenchConfig('jetstream3.crossbench',
                          'jetstream_3',
                          estimated_runtime=estimated_runtime,
                          flags=flags)


@_register('jetstream_main.crossbench')
def _jetstream_main_crossbench(estimated_runtime: int = 180,
                               flags: tuple[str, ...] = ()):
  return CrossbenchConfig('jetstream_main.crossbench',
                          'jetstream_main',
                          estimated_runtime=estimated_runtime,
                          flags=flags)


@_register('jetstream3-turbolev_future.crossbench')
def _jetstream3_turbolev_future_crossbench(estimated_runtime: int = 180,
                                           flags: tuple[str, ...] = ()):
  flags += ('--js-flags=--turbolev-future', )
  return CrossbenchConfig('jetstream3-turbolev_future.crossbench',
                          'jetstream_3',
                          estimated_runtime=estimated_runtime,
                          flags=flags)


# LoadLine:
@_register('loadline_phone.crossbench')
def _loadline_phone_crossbench(estimated_runtime: int = 7000,
                               flags: tuple[str, ...] = ()):
  return CrossbenchConfig('loadline_phone.crossbench',
                          'loadline-phone-fast',
                          estimated_runtime=estimated_runtime,
                          flags=flags)


@_register('loadline_tablet.crossbench')
def _loadline_tablet_crossbench(estimated_runtime: int = 3600,
                                flags: tuple[str, ...] = ()):
  return CrossbenchConfig('loadline_tablet.crossbench',
                          'loadline-tablet-fast',
                          estimated_runtime=estimated_runtime,
                          flags=flags)


@_register('loadline2_phone.crossbench')
def _loadline2_phone_crossbench(estimated_runtime: int = 1000,
                                flags: tuple[str, ...] = ()):
  return CrossbenchConfig('loadline2_phone.crossbench',
                          'loadline2-phone',
                          estimated_runtime=estimated_runtime,
                          flags=flags)


# Webview:
@_register('loading.crossbench')
def _crossbench_loading(estimated_runtime: int = 750,
                        flags: tuple[str, ...] = ()):
  return CrossbenchConfig('loading.crossbench',
                          'loading',
                          estimated_runtime=estimated_runtime,
                          flags=flags)


@_register('embedder.crossbench')
def _crossbench_embedder(estimated_runtime: int = 900,
                         flags: tuple[str, ...] = ()):
  return CrossbenchConfig('embedder.crossbench',
                          'embedder',
                          estimated_runtime=estimated_runtime,
                          flags=flags)


@_register('devtools_frontend.crossbench')
def _devtools_frontend_crossbench(estimated_runtime: int = 60,
                                  flags: tuple[str, ...] = ()):
  return CrossbenchConfig('devtools_frontend.crossbench',
                          'devtools_frontend',
                          estimated_runtime=estimated_runtime,
                          flags=flags)


FUCHSIA_EXEC_ARGS: dict[str, list[str] | None] = {
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

# TODO(zijiehe): Fuchsia should check the os version, i.e. --os-check=check, but
# perf test run multiple suites in sequential and the os checks are performed
# multiple times. Currently there isn't a simple way to check only once at the
# beginning of the test.
# See the revision:
# https://crsrc.org/c/tools/perf/core/bot_platforms.py
#   ;drc=93a804bc8c5871e1fb70a762e461d787749cb2d7;l=470
_COMMON_FUCHSIA_ARGS = ['-d', '--os-check=ignore']
for board, path_parts in _IMAGE_PATHS.items():
  FUCHSIA_EXEC_ARGS[board] = _COMMON_FUCHSIA_ARGS


def CreateLegacySchedule() -> set[_PerfPlatform]:
  all_platforms: set[_PerfPlatform] = set()
  _CROSSBENCH_JETSTREAM_SPEEDOMETER = frozenset([
      _jetstream2_crossbench(),
      _speedometer3_crossbench(),
  ])

  _CROSSBENCH_MOTIONMARK_SPEEDOMETER = frozenset([
      _motionmark1_3_crossbench(),
      _speedometer3_crossbench(),
  ])

  _CROSSBENCH_BENCHMARKS_ALL = frozenset([
      _speedometer3_crossbench(),
      _motionmark1_3_crossbench(),
      _jetstream2_crossbench(),
      _jetstream3_crossbench(),
      _jetstream3_turbolev_future_crossbench(),
  ])

  # TODO(crbug.com/338630584): Remove it when other benchmarks can be run on
  # Android.
  _CROSSBENCH_ANDROID = frozenset([
      _speedometer3_crossbench(flags=('--fileserver', )),
      _loadline_phone_crossbench(flags=('--cool-down-threshold=moderate', )),
  ])

  # TODO(crbug.com/409326154): Enable crossbench variant when supported.
  # TODO(crbug.com/409571674): Remove --debug flag.
  _CROSSBENCH_PIXEL9 = frozenset([
      # _jetstream2_crossbench(flags=('--fileserver', '--debug')),
      _motionmark1_3_crossbench(flags=('--fileserver', '--debug')),
      _speedometer3_crossbench(flags=('--fileserver', '--debug')),
      _speedometer3_a11y_crossbench(flags=('--fileserver', '--debug')),
      _loadline_phone_crossbench(flags=(
          '--cool-down-threshold=moderate',
          '--debug',
      )),
      _loadline2_phone_crossbench(flags=('--debug', )),
  ])

  _CROSSBENCH_ANDROID_AL_BRYA = frozenset([
      _speedometer3_crossbench(flags=('--fileserver', '--debug')),
      _motionmark1_3_crossbench(flags=('--fileserver', '--debug')),
  ])

  _CROSSBENCH_ANDROID_AL = frozenset([
      _speedometer3_crossbench(flags=('--fileserver', '--debug')),
  ])

  _CROSSBENCH_TANGOR = frozenset([
      _loadline_tablet_crossbench(flags=('--cool-down-threshold=moderate', )),
  ])

  # pylint: disable=line-too-long
  _CROSSBENCH_WEBVIEW = frozenset([
      _crossbench_loading(
          flags=(
              '--wpr=crossbench_android_loading_000.wprgo',
              '--probe=chrome_histograms:{"baseline":false,"metrics":'
              '{"Android.WebView.Startup.CreationTime.StartChromiumLocked":["mean"],'
              '"Android.WebView.Startup.CreationTime.Stage1.FactoryInit":["mean"],'
              '"PageLoad.PaintTiming.NavigationToFirstContentfulPaint":["mean"]}}',
              '--repetitions=50',
              '--cool-down-threshold=moderate',
              '--stories=cnn',
          )),
      _crossbench_embedder(
          flags=(
              '--wpr=crossbench_android_embedder_000.wprgo',
              '--skip-wpr-script-injection',
              '--embedder=../../clank/android_webview/tools/crossbench_config/cipd/arm64/Velvet_arm64.apk',
              '--splashscreen=skip',
              '--cuj-config=../../third_party/crossbench/config/team/woa/embedder_cuj_config.hjson',
              '--probe-config=../../clank/android_webview/tools/crossbench_config/'
              'agsa_probe_config.hjson',
              '--repetitions=50',
              '--cool-down-threshold=moderate',
              '--http-request-timeout=10s',
              '--ignore-partial-failures',
              '--embedder-process-name=googleapp',
              '--embedder-setup-command-config=../../clank/android_webview/tools/crossbench_config/'
              'agsa_setup_config.hjson',
              '--embedder-drop-caches',
          )),
  ])
  # pylint: enable=line-too-long

  _CHROME_HEALTH_BENCHMARK_CONFIGS_DESKTOP = PerfSuite(
      [_TelemetryConfig('system_health.common_desktop')])

  _LINUX_BENCHMARK_CONFIGS = PerfSuite(_OFFICIAL_BENCHMARK_CONFIGS).Remove([
      'v8.runtime_stats.top_25',
  ]).Add([
      'blink_perf.svg',
      'blink_perf.paint',
  ])

  _LINUX_EXECUTABLE_CONFIGS = frozenset([
      # TODO(crbug.com/40562709): Add views_perftests.
      _base_perftests(200),
      _load_library_perf_tests(),
      _tint_benchmark(),
      _tracing_perftests(5),
  ])
  _LINUX_R350_BENCHMARK_CONFIGS = PerfSuite(_LINUX_BENCHMARK_CONFIGS).Remove([
      'rendering.desktop',
      'rendering.desktop.notracing',
      'system_health.common_desktop',
  ])
  # For linux-perf, which runs benchmarks that are skipped on linux-r350-perf.
  _LINUX_GPU_BENCHMARK_CONFIGS = PerfSuite([
      _TelemetryConfig('rendering.desktop'),
      _TelemetryConfig('rendering.desktop.notracing'),
      _TelemetryConfig('system_health.common_desktop'),
  ])
  _MAC_INTEL_BENCHMARK_CONFIGS = PerfSuite(_OFFICIAL_BENCHMARK_CONFIGS).Remove([
      'v8.runtime_stats.top_25',
      'rendering.desktop',
  ])
  _MAC_INTEL_EXECUTABLE_CONFIGS = frozenset([
      _base_perftests(300),
      _dawn_perf_tests(330),
      _tint_benchmark(),
      _views_perftests(),
      _load_library_perf_tests(),
  ])
  _MAC_M1_MINI_2020_BENCHMARK_CONFIGS = PerfSuite(
      _OFFICIAL_BENCHMARK_CONFIGS).Remove([
          'v8.runtime_stats.top_25',
      ]).Add([
          'jetstream2-no-field-trials',
          'speedometer3-no-field-trials',
      ]).Repeat([
          'rendering.desktop.notracing',
      ], 2).Repeat([
          'speedometer3',
          'speedometer3-no-field-trials',
      ], 6).Repeat([
          'jetstream2',
          'jetstream2-no-field-trials',
      ], 11)
  _MAC_M1_MINI_2020_PGO_BENCHMARK_CONFIGS = PerfSuite([
      _TelemetryConfig('jetstream2', pageset_repeat=11),
      _TelemetryConfig('speedometer3', pageset_repeat=22),
      _TelemetryConfig('rendering.desktop.notracing'),
  ])
  _MAC_M1_MINI_2020_NO_BRP_BENCHMARK_CONFIGS = PerfSuite([
      _TelemetryConfig('speedometer3', pageset_repeat=2),
      _TelemetryConfig('rendering.desktop.notracing', pageset_repeat=2),
  ])
  _MAC_M1_PRO_BENCHMARK_CONFIGS = PerfSuite([
      _TelemetryConfig('jetstream2'),
      _TelemetryConfig('speedometer2'),
      _TelemetryConfig('speedometer3'),
      _TelemetryConfig('rendering.desktop.notracing'),
  ])
  _MAC_M1_MINI_2020_EXECUTABLE_CONFIGS = frozenset([
      _base_perftests(300),
      _dawn_perf_tests(330),
      _tint_benchmark(),
      _views_perftests(),
  ])
  _MAC_M2_PRO_BENCHMARK_CONFIGS = PerfSuite(_OFFICIAL_BENCHMARK_CONFIGS).Remove(
      [
          'v8.runtime_stats.top_25',
      ])
  _MAC_M3_PRO_BENCHMARK_CONFIGS = PerfSuite([])
  _MAC_M4_MINI_BENCHMARK_CONFIGS = PerfSuite(_OFFICIAL_BENCHMARK_CONFIGS)

  _WIN_10_BENCHMARK_CONFIGS = PerfSuite(_OFFICIAL_BENCHMARK_CONFIGS).Remove([
      'v8.runtime_stats.top_25',
  ])
  _WIN_10_EXECUTABLE_CONFIGS = frozenset([
      _base_perftests(200),
      _components_perftests(125),
      _dawn_perf_tests(600),
      _views_perftests(),
  ])
  _WIN_10_LOW_END_BENCHMARK_CONFIGS = PerfSuite(_OFFICIAL_BENCHMARK_CONFIGS)
  _WIN_10_LOW_END_HP_CANDIDATE_BENCHMARK_CONFIGS = PerfSuite([
      _TelemetryConfig('v8.browsing_desktop'),
      _TelemetryConfig('rendering.desktop', abridged=True),
  ])
  _WIN_10_AMD_LAPTOP_BENCHMARK_CONFIGS = PerfSuite([
      _TelemetryConfig('jetstream2'),
      _TelemetryConfig('speedometer2'),
      _TelemetryConfig('speedometer3'),
  ])
  _WIN_11_BENCHMARK_CONFIGS = PerfSuite(_OFFICIAL_BENCHMARK_CONFIGS).Remove([
      'rendering.desktop',
      'rendering.desktop.notracing',
      'system_health.common_desktop',
      'v8.runtime_stats.top_25',
  ])
  _WIN_11_EXECUTABLE_CONFIGS = frozenset([
      _base_perftests(200),
      _components_perftests(125),
      _dawn_perf_tests(600),
      _tint_benchmark(),
      _views_perftests(),
  ])
  _WIN_ARM64_BENCHMARK_CONFIGS = PerfSuite([
      _TelemetryConfig('blink_perf.dom'),
      _TelemetryConfig('jetstream2'),
      _TelemetryConfig('media.desktop'),
      _TelemetryConfig('rendering.desktop', abridged=True),
      _TelemetryConfig('rendering.desktop.notracing'),
      _TelemetryConfig('speedometer3'),
      _TelemetryConfig('system_health.common_desktop'),
      _TelemetryConfig('v8.browsing_desktop'),
  ])
  _WIN_ARM64_EXECUTABLE_CONFIGS = frozenset([
      _base_perftests(200),
      _components_perftests(125),
      _views_perftests(),
  ])
  _FALCON_BENCHMARK_CONFIGS = PerfSuite([
      _TelemetryConfig('blink_perf.dom'),
      _TelemetryConfig('jetstream2'),
      _TelemetryConfig('media.desktop'),
      _TelemetryConfig('rendering.desktop', abridged=True),
      _TelemetryConfig('rendering.desktop.notracing'),
      _TelemetryConfig('speedometer3'),
      _TelemetryConfig('system_health.common_desktop'),
      _TelemetryConfig('v8.browsing_desktop'),
  ])
  _FALCON_EXECUTABLE_CONFIGS = frozenset([
      _base_perftests(200),
      _components_perftests(125),
      _dawn_perf_tests(600),
      _tint_benchmark(),
      _views_perftests(),
  ])
  _ANDROID_GO_BENCHMARK_CONFIGS = PerfSuite([
      _TelemetryConfig('system_health.memory_mobile'),
      _TelemetryConfig('system_health.common_mobile'),
      _TelemetryConfig('startup.mobile'),
      _TelemetryConfig('system_health.webview_startup'),
      _TelemetryConfig('v8.browsing_mobile'),
      _TelemetryConfig('speedometer3'),
  ])
  _ANDROID_DEFAULT_EXECUTABLE_CONFIGS = frozenset([
      _components_perftests(60),
  ])
  _ANDROID_GO_WEBVIEW_BENCHMARK_CONFIGS = _ANDROID_GO_BENCHMARK_CONFIGS
  _ANDROID_PIXEL4_BENCHMARK_CONFIGS = PerfSuite(_OFFICIAL_BENCHMARK_CONFIGS)
  _ANDROID_PIXEL4_WEBVIEW_BENCHMARK_CONFIGS = PerfSuite(
      _OFFICIAL_BENCHMARK_CONFIGS).Remove([
          'jetstream2',
          'v8.browsing_mobile-future',
      ])
  _ANDROID_PIXEL6_BENCHMARK_CONFIGS = PerfSuite(
      _OFFICIAL_BENCHMARK_CONFIGS).Add(
          [_TelemetryConfig('system_health.scroll_jank_mobile')]).Repeat([
              'speedometer3',
          ], 4)
  _ANDROID_PIXEL6_PGO_BENCHMARK_CONFIGS = PerfSuite([
      _TelemetryConfig('system_health.common_mobile'),
      _TelemetryConfig('jetstream2'),
      _TelemetryConfig('rendering.mobile'),
      _TelemetryConfig('speedometer2'),
      _TelemetryConfig('speedometer3', pageset_repeat=16),
  ])
  # TODO(crbug.com/409326154): Remove these for the crossbench variants when
  # supported.
  _ANDROID_PIXEL9_BENCHMARK_CONFIGS = PerfSuite([
      _TelemetryConfig('jetstream2'),
  ])
  # Android Desktop (AL)
  _ANDROID_AL_BRYA_BENCHMARK_CONFIGS = PerfSuite([
      _TelemetryConfig('jetstream2'),
      _TelemetryConfig('speedometer2'),
  ])
  _ANDROID_AL_BRYA_EXECUTABLE_CONFIGS = frozenset([
      _web_tests_cuj(),
  ])
  _ANDROID_AL_BENCHMARK_CONFIGS = PerfSuite([
      _TelemetryConfig('rendering.mobile'),
  ])

  _CHROMEOS_KEVIN_FYI_BENCHMARK_CONFIGS = PerfSuite(
      [_TelemetryConfig('rendering.desktop')])
  _FUCHSIA_PERF_SMARTDISPLAY_BENCHMARK_CONFIGS = PerfSuite([
      _TelemetryConfig('speedometer2'),
      _TelemetryConfig('media.mobile'),
      _TelemetryConfig('v8.browsing_mobile'),
  ])
  _LINUX_PERF_FYI_BENCHMARK_CONFIGS = PerfSuite([
      _TelemetryConfig('speedometer2'),
      _TelemetryConfig('speedometer3'),
  ])

  # Linux
  new_platform = _PerfPlatform('linux-perf',
                               ('Ubuntu-22.04, Precision 3930 Rack, '
                                'NVIDIA GeForce GTX 1660'),
                               _LINUX_GPU_BENCHMARK_CONFIGS,
                               7,
                               'linux',
                               executables=_LINUX_EXECUTABLE_CONFIGS,
                               crossbench=_CROSSBENCH_BENCHMARKS_ALL)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('linux-perf-pgo',
                               'Ubuntu-18.04, 8 core, NVIDIA Quadro P400',
                               _LINUX_BENCHMARK_CONFIGS,
                               26,
                               'linux',
                               executables=_LINUX_EXECUTABLE_CONFIGS,
                               pinpoint_only=True)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('linux-perf-rel',
                               'Ubuntu-18.04, 8 core, NVIDIA Quadro P400',
                               _CHROME_HEALTH_BENCHMARK_CONFIGS_DESKTOP,
                               2,
                               'linux',
                               executables=_LINUX_EXECUTABLE_CONFIGS)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('linux-r350-perf',
                               'Ubuntu-22.04, 16 core',
                               _LINUX_R350_BENCHMARK_CONFIGS,
                               30,
                               'linux',
                               executables=_LINUX_EXECUTABLE_CONFIGS,
                               crossbench=_CROSSBENCH_BENCHMARKS_ALL
                               | {_devtools_frontend_crossbench()})
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('linux-falcon-rak-5070-perf',
                               'Linux Falcon RAK 5070',
                               _FALCON_BENCHMARK_CONFIGS,
                               1,
                               'linux',
                               executables=_FALCON_EXECUTABLE_CONFIGS,
                               crossbench=_CROSSBENCH_BENCHMARKS_ALL)

  # Mac
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('mac-intel-perf',
                               'Mac Mini 8,1, Core i7 3.2 GHz',
                               _MAC_INTEL_BENCHMARK_CONFIGS,
                               24,
                               'mac',
                               executables=_MAC_INTEL_EXECUTABLE_CONFIGS,
                               crossbench=_CROSSBENCH_BENCHMARKS_ALL)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('mac-m1_mini_2020-perf',
                               'Mac M1 Mini 2020',
                               _MAC_M1_MINI_2020_BENCHMARK_CONFIGS,
                               28,
                               'mac',
                               executables=_MAC_M1_MINI_2020_EXECUTABLE_CONFIGS,
                               crossbench=_CROSSBENCH_BENCHMARKS_ALL
                               | {_devtools_frontend_crossbench()})
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('mac-m1_mini_2020-perf-pgo',
                               'Mac M1 Mini 2020',
                               _MAC_M1_MINI_2020_PGO_BENCHMARK_CONFIGS,
                               7,
                               'mac',
                               crossbench=_CROSSBENCH_BENCHMARKS_ALL)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('mac-m1_mini_2020-no-brp-perf',
                               'Mac M1 Mini 2020 with BRP disabled',
                               _MAC_M1_MINI_2020_NO_BRP_BENCHMARK_CONFIGS, 20,
                               'mac')
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('mac-m1-pro-perf',
                               'Mac M1 PRO 2020',
                               _MAC_M1_PRO_BENCHMARK_CONFIGS,
                               4,
                               'mac',
                               crossbench=_CROSSBENCH_BENCHMARKS_ALL)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('mac-m2-pro-perf',
                               'Mac M2 PRO Baremetal ARM',
                               _MAC_M2_PRO_BENCHMARK_CONFIGS,
                               20,
                               'mac',
                               crossbench=_CROSSBENCH_BENCHMARKS_ALL)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('mac-m3-pro-perf',
                               'Mac M3 PRO ARM',
                               _MAC_M3_PRO_BENCHMARK_CONFIGS,
                               4,
                               'mac',
                               crossbench=_CROSSBENCH_BENCHMARKS_ALL)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('mac-m4-mini-perf',
                               'Mac M4 mini ARM',
                               _MAC_M4_MINI_BENCHMARK_CONFIGS,
                               25,
                               'mac',
                               crossbench=_CROSSBENCH_BENCHMARKS_ALL)
  all_platforms.add(new_platform)
  # Win
  new_platform = _PerfPlatform(
      'win-10_laptop_low_end-perf',
      'Low end windows 10 HP laptops. HD Graphics 5500, x86-64-i3-5005U, '
      'SSD, 4GB RAM.', _WIN_10_LOW_END_BENCHMARK_CONFIGS, 15, 'win')
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform(
      'win-10_laptop_low_end-perf-pgo',
      'Low end windows 10 HP laptops. HD Graphics 5500, x86-64-i3-5005U, '
      'SSD, 4GB RAM.',
      _WIN_10_LOW_END_BENCHMARK_CONFIGS,
      # TODO(crbug.com/40218037): Increase the count back to 46 when issue fixed.
      40,
      'win',
      pinpoint_only=True)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform(
      'win-10-perf',
      'Windows Intel HD 630 towers, Core i7-7700 3.6 GHz, 16GB RAM,'
      ' Intel Kaby Lake HD Graphics 630',
      _WIN_10_BENCHMARK_CONFIGS,
      18,
      'win',
      executables=_WIN_10_EXECUTABLE_CONFIGS,
      crossbench=_CROSSBENCH_BENCHMARKS_ALL)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform(
      'win-10-perf-pgo',
      'Windows Intel HD 630 towers, Core i7-7700 3.6 GHz, 16GB RAM,'
      ' Intel Kaby Lake HD Graphics 630',
      _WIN_10_BENCHMARK_CONFIGS,
      18,
      'win',
      executables=_WIN_10_EXECUTABLE_CONFIGS,
      pinpoint_only=True)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('win-10_amd_laptop-perf',
                               'Windows 10 Laptop with AMD chipset.',
                               _WIN_10_AMD_LAPTOP_BENCHMARK_CONFIGS,
                               3,
                               'win',
                               crossbench=_CROSSBENCH_JETSTREAM_SPEEDOMETER)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('win-10_amd_laptop-perf-pgo',
                               'Windows 10 Laptop with AMD chipset.',
                               _WIN_10_AMD_LAPTOP_BENCHMARK_CONFIGS,
                               3,
                               'win',
                               pinpoint_only=True)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform(
      'win-11-perf',
      'Windows Dell PowerEdge R350',
      _WIN_11_BENCHMARK_CONFIGS,
      20,
      'win',
      executables=_WIN_11_EXECUTABLE_CONFIGS,
      crossbench=_CROSSBENCH_BENCHMARKS_ALL
      | {_speedometer3_a11y_crossbench(),
         _devtools_frontend_crossbench()})
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('win-11-perf-pgo',
                               'Windows Dell PowerEdge R350',
                               _WIN_11_BENCHMARK_CONFIGS,
                               26,
                               'win',
                               executables=_WIN_11_EXECUTABLE_CONFIGS,
                               pinpoint_only=True)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('win-falcon-rak-5070-perf',
                               'Windows Falcon RAK 5070',
                               _FALCON_BENCHMARK_CONFIGS,
                               1,
                               'win',
                               executables=_FALCON_EXECUTABLE_CONFIGS,
                               crossbench=_CROSSBENCH_BENCHMARKS_ALL)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('win-arm64-snapdragon-elite-perf',
                               'Windows Dell Snapdragon Elite',
                               _OFFICIAL_BENCHMARK_CONFIGS,
                               28,
                               'win',
                               executables=_WIN_ARM64_EXECUTABLE_CONFIGS,
                               crossbench=_CROSSBENCH_BENCHMARKS_ALL)
  all_platforms.add(new_platform)

  # Android
  new_platform = _PerfPlatform(
      name='android-brya-kano-i5-8gb-perf',
      description='Brya SKU kano_12th_Gen_IntelR_CoreTM_i5_1235U_8GB',
      # We have enough resources to run at least 7 shards, but currently only
      # have enough benchmarks to fill 4 shards, so setting num_shards=4 to
      # avoid wasting resources.
      num_shards=4,
      benchmark_configs=_ANDROID_AL_BRYA_BENCHMARK_CONFIGS,
      platform_os='android',
      executables=_ANDROID_AL_BRYA_EXECUTABLE_CONFIGS,
      crossbench=_CROSSBENCH_ANDROID_AL)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform(name='android-corsola-steelix-8gb-perf',
                               description='Corsola SKU steelix_MT8186_8GB',
                               num_shards=7,
                               benchmark_configs=_ANDROID_AL_BENCHMARK_CONFIGS,
                               platform_os='android',
                               executables=None,
                               crossbench=_CROSSBENCH_ANDROID_AL)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform(
      name='android-nissa-uldren-8gb-perf',
      description='Nissa SKU uldren_99C4LZ/Q1XT/6W_8GB',
      num_shards=7,
      benchmark_configs=_ANDROID_AL_BENCHMARK_CONFIGS,
      platform_os='android',
      executables=None,
      crossbench=_CROSSBENCH_ANDROID_AL)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('android-pixel4-perf',
                               'Android R',
                               _ANDROID_PIXEL4_BENCHMARK_CONFIGS,
                               44,
                               'android',
                               executables=_ANDROID_DEFAULT_EXECUTABLE_CONFIGS)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('android-pixel4-perf-pgo',
                               'Android R',
                               _ANDROID_PIXEL4_BENCHMARK_CONFIGS,
                               28,
                               'android',
                               executables=_ANDROID_DEFAULT_EXECUTABLE_CONFIGS,
                               pinpoint_only=True)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('android-pixel4_webview-perf',
                               'Android R',
                               _ANDROID_PIXEL4_WEBVIEW_BENCHMARK_CONFIGS,
                               23,
                               'android',
                               crossbench=_CROSSBENCH_WEBVIEW)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('android-pixel4_webview-perf-pgo',
                               'Android R',
                               _ANDROID_PIXEL4_WEBVIEW_BENCHMARK_CONFIGS,
                               20,
                               'android',
                               crossbench=_CROSSBENCH_WEBVIEW)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('android-pixel6-perf',
                               'Android U',
                               _ANDROID_PIXEL6_BENCHMARK_CONFIGS,
                               14,
                               'android',
                               executables=_ANDROID_DEFAULT_EXECUTABLE_CONFIGS,
                               crossbench=_CROSSBENCH_ANDROID)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('android-pixel6-perf-pgo',
                               'Android U',
                               _ANDROID_PIXEL6_PGO_BENCHMARK_CONFIGS,
                               8,
                               'android',
                               executables=_ANDROID_DEFAULT_EXECUTABLE_CONFIGS,
                               crossbench=_CROSSBENCH_ANDROID)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('android-pixel6-pro-perf',
                               'Android T',
                               _OFFICIAL_BENCHMARK_CONFIGS,
                               10,
                               'android',
                               executables=_ANDROID_DEFAULT_EXECUTABLE_CONFIGS)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('android-pixel6-pro-perf-pgo',
                               'Android T',
                               _OFFICIAL_BENCHMARK_CONFIGS,
                               16,
                               'android',
                               executables=_ANDROID_DEFAULT_EXECUTABLE_CONFIGS,
                               pinpoint_only=True)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('android-pixel-fold-perf',
                               'Android U',
                               _OFFICIAL_BENCHMARK_CONFIGS,
                               10,
                               'android',
                               executables=_ANDROID_DEFAULT_EXECUTABLE_CONFIGS)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('android-pixel-tangor-perf',
                               'Android U',
                               _OFFICIAL_BENCHMARK_CONFIGS,
                               8,
                               'android',
                               executables=_ANDROID_DEFAULT_EXECUTABLE_CONFIGS,
                               crossbench=_CROSSBENCH_TANGOR)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('android-go-wembley-perf', 'Android U',
                               _ANDROID_GO_BENCHMARK_CONFIGS, 11, 'android')
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('android-go-wembley_webview-perf', 'Android U',
                               _ANDROID_GO_WEBVIEW_BENCHMARK_CONFIGS, 5,
                               'android')
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('android-pixel9-perf',
                               'Android B',
                               _ANDROID_PIXEL9_BENCHMARK_CONFIGS,
                               4,
                               'android',
                               executables=_ANDROID_DEFAULT_EXECUTABLE_CONFIGS,
                               crossbench=_CROSSBENCH_PIXEL9)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('android-pixel9-pro-perf',
                               'Android B',
                               _ANDROID_PIXEL9_BENCHMARK_CONFIGS,
                               4,
                               'android',
                               executables=_ANDROID_DEFAULT_EXECUTABLE_CONFIGS,
                               crossbench=_CROSSBENCH_PIXEL9)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('android-pixel9-pro-xl-perf',
                               'Android B',
                               _ANDROID_PIXEL9_BENCHMARK_CONFIGS,
                               4,
                               'android',
                               executables=_ANDROID_DEFAULT_EXECUTABLE_CONFIGS,
                               crossbench=_CROSSBENCH_PIXEL9)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('android-pixel25-ultra-perf',
                               'Android B',
                               _ANDROID_PIXEL9_BENCHMARK_CONFIGS,
                               4,
                               'android',
                               executables=_ANDROID_DEFAULT_EXECUTABLE_CONFIGS,
                               crossbench=_CROSSBENCH_PIXEL9)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('android-pixel25-ultra-xl-perf',
                               'Android B',
                               _ANDROID_PIXEL9_BENCHMARK_CONFIGS,
                               3,
                               'android',
                               executables=_ANDROID_DEFAULT_EXECUTABLE_CONFIGS,
                               crossbench=_CROSSBENCH_PIXEL9)
  # Cros
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('fuchsia-perf-nsn',
                               '',
                               _FUCHSIA_PERF_SMARTDISPLAY_BENCHMARK_CONFIGS,
                               1,
                               'fuchsia',
                               is_fyi=True)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('fuchsia-perf-shk',
                               '',
                               _FUCHSIA_PERF_SMARTDISPLAY_BENCHMARK_CONFIGS,
                               1,
                               'fuchsia',
                               is_fyi=True)

  # FYI bots
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('win-10_laptop_low_end-perf_HP-Candidate',
                               'HP 15-BS121NR Laptop Candidate',
                               _WIN_10_LOW_END_HP_CANDIDATE_BENCHMARK_CONFIGS,
                               1,
                               'win',
                               is_fyi=True)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('chromeos-kevin-perf-fyi',
                               '',
                               _CHROMEOS_KEVIN_FYI_BENCHMARK_CONFIGS,
                               4,
                               'chromeos',
                               is_fyi=True)
  all_platforms.add(new_platform)
  new_platform = _PerfPlatform('linux-perf-fyi',
                               '',
                               _LINUX_PERF_FYI_BENCHMARK_CONFIGS,
                               1,
                               'linux',
                               crossbench=_CROSSBENCH_BENCHMARKS_ALL
                               | {_devtools_frontend_crossbench()},
                               is_fyi=True)
  all_platforms.add(new_platform)

  # Silence unused variable warnings:
  del _CROSSBENCH_MOTIONMARK_SPEEDOMETER
  del _CROSSBENCH_ANDROID_AL_BRYA
  del _WIN_ARM64_BENCHMARK_CONFIGS
  return all_platforms


PLATFORM_INFO = {
    'linux-perf': {
        'description': ('Ubuntu-22.04, Precision 3930 Rack, '
                        'NVIDIA GeForce GTX 1660'),
        'num_shards':
        7,
        'platform_os':
        'linux',
        'is_fyi':
        False
    },
    'linux-perf-pgo': {
        'description': 'Ubuntu-18.04, 8 core, NVIDIA Quadro P400',
        'num_shards': 26,
        'platform_os': 'linux',
        'is_fyi': False,
        'pinpoint_only': True
    },
    'linux-perf-rel': {
        'description': 'Ubuntu-18.04, 8 core, NVIDIA Quadro P400',
        'num_shards': 2,
        'platform_os': 'linux',
        'is_fyi': False
    },
    'linux-r350-perf': {
        'description': 'Ubuntu-22.04, 16 core',
        'num_shards': 30,
        'platform_os': 'linux',
        'is_fyi': False
    },
    'linux-falcon-rak-5070-perf': {
        'description': 'Linux Falcon RAK 5070',
        'num_shards': 1,
        'platform_os': 'linux',
        'is_fyi': False
    },
    'mac-intel-perf': {
        'description': 'Mac Mini 8,1, Core i7 3.2 GHz',
        'num_shards': 24,
        'platform_os': 'mac',
        'is_fyi': False
    },
    'mac-m1_mini_2020-perf': {
        'description': 'Mac M1 Mini 2020',
        'num_shards': 28,
        'platform_os': 'mac',
        'is_fyi': False
    },
    'mac-m1_mini_2020-perf-pgo': {
        'description': 'Mac M1 Mini 2020',
        'num_shards': 7,
        'platform_os': 'mac',
        'is_fyi': False
    },
    'mac-m1_mini_2020-no-brp-perf': {
        'description': 'Mac M1 Mini 2020 with BRP disabled',
        'num_shards': 20,
        'platform_os': 'mac',
        'is_fyi': False
    },
    'mac-m1-pro-perf': {
        'description': 'Mac M1 PRO 2020',
        'num_shards': 4,
        'platform_os': 'mac',
        'is_fyi': False
    },
    'mac-m2-pro-perf': {
        'description': 'Mac M2 PRO Baremetal ARM',
        'num_shards': 20,
        'platform_os': 'mac',
        'is_fyi': False
    },
    'mac-m3-pro-perf': {
        'description': 'Mac M3 PRO ARM',
        'num_shards': 4,
        'platform_os': 'mac',
        'is_fyi': False
    },
    'mac-m4-mini-perf': {
        'description': 'Mac M4 mini ARM',
        'num_shards': 25,
        'platform_os': 'mac',
        'is_fyi': False
    },
    'win-10_laptop_low_end-perf': {
        'description': ('Low end windows 10 HP laptops. HD Graphics 5500, '
                        'x86-64-i3-5005U, SSD, 4GB RAM.'),
        'num_shards':
        15,
        'platform_os':
        'win',
        'is_fyi':
        False
    },
    'win-10_laptop_low_end-perf-pgo': {
        'description': ('Low end windows 10 HP laptops. HD Graphics 5500, '
                        'x86-64-i3-5005U, SSD, 4GB RAM.'),
        'num_shards':
        40,
        'platform_os':
        'win',
        'is_fyi':
        False,
        'pinpoint_only':
        True
    },
    'win-10-perf': {
        'description': ('Windows Intel HD 630 towers, Core i7-7700 3.6 GHz, '
                        '16GB RAM, Intel Kaby Lake HD Graphics 630'),
        'num_shards':
        18,
        'platform_os':
        'win',
        'is_fyi':
        False
    },
    'win-10-perf-pgo': {
        'description': ('Windows Intel HD 630 towers, Core i7-7700 3.6 GHz, '
                        '16GB RAM, Intel Kaby Lake HD Graphics 630'),
        'num_shards':
        18,
        'platform_os':
        'win',
        'is_fyi':
        False,
        'pinpoint_only':
        True
    },
    'win-10_amd_laptop-perf': {
        'description': 'Windows 10 Laptop with AMD chipset.',
        'num_shards': 3,
        'platform_os': 'win',
        'is_fyi': False
    },
    'win-10_amd_laptop-perf-pgo': {
        'description': 'Windows 10 Laptop with AMD chipset.',
        'num_shards': 3,
        'platform_os': 'win',
        'is_fyi': False,
        'pinpoint_only': True
    },
    'win-11-perf': {
        'description': 'Windows Dell PowerEdge R350',
        'num_shards': 20,
        'platform_os': 'win',
        'is_fyi': False
    },
    'win-11-perf-pgo': {
        'description': 'Windows Dell PowerEdge R350',
        'num_shards': 26,
        'platform_os': 'win',
        'is_fyi': False,
        'pinpoint_only': True
    },
    'win-falcon-rak-5070-perf': {
        'description': 'Windows Falcon RAK 5070',
        'num_shards': 1,
        'platform_os': 'win',
        'is_fyi': False
    },
    'win-arm64-snapdragon-elite-perf': {
        'description': 'Windows Dell Snapdragon Elite',
        'num_shards': 28,
        'platform_os': 'win',
        'is_fyi': False
    },
    'android-brya-kano-i5-8gb-perf': {
        'description': 'Brya SKU kano_12th_Gen_IntelR_CoreTM_i5_1235U_8GB',
        'num_shards': 4,
        'platform_os': 'android',
        'is_fyi': False
    },
    'android-corsola-steelix-8gb-perf': {
        'description': 'Corsola SKU steelix_MT8186_8GB',
        'num_shards': 7,
        'platform_os': 'android',
        'is_fyi': False
    },
    'android-nissa-uldren-8gb-perf': {
        'description': 'Nissa SKU uldren_99C4LZ/Q1XT/6W_8GB',
        'num_shards': 7,
        'platform_os': 'android',
        'is_fyi': False
    },
    'android-pixel4-perf': {
        'description': 'Android R',
        'num_shards': 44,
        'platform_os': 'android',
        'is_fyi': False
    },
    'android-pixel4-perf-pgo': {
        'description': 'Android R',
        'num_shards': 28,
        'platform_os': 'android',
        'is_fyi': False,
        'pinpoint_only': True
    },
    'android-pixel4_webview-perf': {
        'description': 'Android R',
        'num_shards': 23,
        'platform_os': 'android',
        'is_fyi': False
    },
    'android-pixel4_webview-perf-pgo': {
        'description': 'Android R',
        'num_shards': 20,
        'platform_os': 'android',
        'is_fyi': False
    },
    'android-pixel6-perf': {
        'description': 'Android U',
        'num_shards': 14,
        'platform_os': 'android',
        'is_fyi': False
    },
    'android-pixel6-perf-pgo': {
        'description': 'Android U',
        'num_shards': 8,
        'platform_os': 'android',
        'is_fyi': False
    },
    'android-pixel6-pro-perf': {
        'description': 'Android T',
        'num_shards': 10,
        'platform_os': 'android',
        'is_fyi': False
    },
    'android-pixel6-pro-perf-pgo': {
        'description': 'Android T',
        'num_shards': 16,
        'platform_os': 'android',
        'is_fyi': False,
        'pinpoint_only': True
    },
    'android-pixel-fold-perf': {
        'description': 'Android U',
        'num_shards': 10,
        'platform_os': 'android',
        'is_fyi': False
    },
    'android-pixel-tangor-perf': {
        'description': 'Android U',
        'num_shards': 8,
        'platform_os': 'android',
        'is_fyi': False
    },
    'android-go-wembley-perf': {
        'description': 'Android U',
        'num_shards': 11,
        'platform_os': 'android',
        'is_fyi': False
    },
    'android-go-wembley_webview-perf': {
        'description': 'Android U',
        'num_shards': 5,
        'platform_os': 'android',
        'is_fyi': False
    },
    'android-pixel9-perf': {
        'description': 'Android B',
        'num_shards': 4,
        'platform_os': 'android',
        'is_fyi': False
    },
    'android-pixel9-pro-perf': {
        'description': 'Android B',
        'num_shards': 4,
        'platform_os': 'android',
        'is_fyi': False
    },
    'android-pixel9-pro-xl-perf': {
        'description': 'Android B',
        'num_shards': 4,
        'platform_os': 'android',
        'is_fyi': False
    },
    'android-pixel25-ultra-perf': {
        'description': 'Android B',
        'num_shards': 4,
        'platform_os': 'android',
        'is_fyi': False
    },
    'android-pixel25-ultra-xl-perf': {
        'description': 'Android B',
        'num_shards': 3,
        'platform_os': 'android',
        'is_fyi': False
    },
    'fuchsia-perf-nsn': {
        'description': '',
        'num_shards': 1,
        'platform_os': 'fuchsia',
        'is_fyi': True
    },
    'fuchsia-perf-shk': {
        'description': '',
        'num_shards': 1,
        'platform_os': 'fuchsia',
        'is_fyi': True
    },
    'win-10_laptop_low_end-perf_HP-Candidate': {
        'description': 'HP 15-BS121NR Laptop Candidate',
        'num_shards': 1,
        'platform_os': 'win',
        'is_fyi': True
    },
    'chromeos-kevin-perf-fyi': {
        'description': '',
        'num_shards': 4,
        'platform_os': 'chromeos',
        'is_fyi': True
    },
    'linux-perf-fyi': {
        'description': '',
        'num_shards': 1,
        'platform_os': 'linux',
        'is_fyi': True
    },
}


def LoadAllScheduleFiles() -> set[_PerfPlatform]:
  schedule_dir = pathlib.Path(__file__).resolve().parent / 'schedule'
  assert schedule_dir.is_dir(), f'Missing schedule directory {schedule_dir}'
  bot_to_csv_configs: dict[str, list[BenchmarkConfig]] = {}
  for file_path in schedule_dir.glob('*.csv'):
    LoadScheduleFile(file_path, bot_to_csv_configs)
  assert bot_to_csv_configs, 'No benchmark schedule configs generated'

  new_platforms = set()
  for name, platform_info in PLATFORM_INFO.items():
    csv_configs = bot_to_csv_configs.get(name, [])

    benchmark_configs = [
        c for c in csv_configs if isinstance(c, TelemetryConfig)
    ]
    executable_configs = frozenset(c for c in csv_configs
                                   if isinstance(c, ExecutableConfig))
    crossbench_configs = frozenset(c for c in csv_configs
                                   if isinstance(c, CrossbenchConfig))

    new_platform = _PerfPlatform(
        name=name,
        description=platform_info['description'],
        benchmark_configs=PerfSuite(benchmark_configs),
        num_shards=platform_info['num_shards'],
        platform_os=platform_info['platform_os'],
        is_fyi=platform_info.get('is_fyi', False),
        run_reference_build=platform_info.get('run_reference_build', False),
        pinpoint_only=platform_info.get('pinpoint_only', False),
        executables=executable_configs,
        crossbench=crossbench_configs)
    new_platforms.add(new_platform)
  return new_platforms


def LoadScheduleFile(file_path: pathlib.Path,
                     configs: dict[str, list[BenchmarkConfig]]):
  name = file_path.stem
  factory = _BENCHMARKS_CONFIG_FACTORIES[name]
  is_telemetry = (factory == _TelemetryConfig)  # pylint: disable=comparison-with-callable)
  contents = file_path.read_text(encoding='utf-8')
  reader = csv.DictReader(io.StringIO(contents), restkey='*flag')
  fieldnames = reader.fieldnames
  assert fieldnames, 'Missing field names'
  has_flags = 'flags' in fieldnames
  seen_bots: set[str] = set()
  for row in reader:
    if flags := ParseFlags(file_path, row, has_flags):
      row['flags'] = flags
    bot = row['bot']
    assert bot not in seen_bots, (
        f'Duplicate bot {bot!r} in schedule file {file_path}')
    seen_bots.add(bot)
    config = _ParseScheduleConfigRow(row, name, factory, is_telemetry)
    configs.setdefault(bot, []).append(config)


def ParseFlags(file_path: pathlib.Path, row, has_flags: bool) -> str | None:
  if extraFlags := row.pop('*flag', None):
    assert has_flags, (
        f'Unexpected extra columns in {file_path}. Extra columns are only '
        'supported if a "flags" column exists.')
    # Merge trailing extra flag values with the explicit flags.
    return ','.join((row['flags'], *extraFlags))
  return None


def _ParseScheduleConfigRow(row, name: str, factory: BenchmarkConfigFactory,
                            is_telemetry: bool) -> BenchmarkConfig:
  repeat = int(row.get('repeat', 1))
  kwargs = {}
  for k, v in row.items():
    if k in ('bot', 'repeat', 'shard'):
      continue
    kwargs[k] = _ParseScheduleConfigValue(k, v)

  if is_telemetry:
    return factory(name, pageset_repeat=repeat, **kwargs)

  config = factory(**kwargs)
  if hasattr(config, 'repeat'):
    config.repeat = repeat
  else:
    assert repeat == 1, f'Cannot use repeat > 1 yet on {name}'

  return config


def _ParseScheduleConfigValue(k, v):
  if k == 'flags':
    v = str(v)
    if v and v[0] == "'":
      raise ValueError(f'Unsupported single quote for flag escaping: {v!r}')
    return tuple(shlex.split(v))
  if v.lower() == 'true':
    return True
  if v.lower() == 'false':
    return False
  return int(v)

# TODO(cbruni): use this to generate all perf configs.
_USE_CSV_SCHEDULE_FILES: Final[bool] = False

ALL_PLATFORMS: set[_PerfPlatform] = set()
if _USE_CSV_SCHEDULE_FILES:
  ALL_PLATFORMS = LoadAllScheduleFiles()
else:
  ALL_PLATFORMS = CreateLegacySchedule()

assert ALL_PLATFORMS, 'No PerfPlatform found'
PLATFORMS_BY_NAME = {p.name: p for p in ALL_PLATFORMS}
FYI_PLATFORMS = {
    p for p in ALL_PLATFORMS if p.is_fyi
}
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
