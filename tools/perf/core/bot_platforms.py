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
import urllib.parse


from typing import Callable, Final, Iterable, Optional, Union


from core import benchmark_finders
from core import benchmark_utils

from telemetry.story import story_filter


_SHARD_MAP_DIR = os.path.join(os.path.dirname(__file__), 'shard_maps')

_ALL_BENCHMARKS_BY_NAMES = dict(
    (b.Name(), b) for b in benchmark_finders.GetAllBenchmarks())
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
            urllib.parse.quote(self._name))


class BenchmarkConfig(object):

  def __init__(self,
               name: str,
               repeat: int = 1,
               flags: tuple[str, ...] = ()) -> None:
    self.name: Final[str] = name
    self.repeat: Final[int] = repeat
    self.flags: Final[tuple[str, ...]] = flags


class TelemetryConfig(BenchmarkConfig):

  def __init__(self,
               benchmark,
               abridged: bool = False,
               repeat: Optional[int] = None):
    """A configuration for a benchmark that helps decide how to shard it.

    Args:
      benchmark: the benchmark.Benchmark object.
      abridged: True if the benchmark should be abridged so fewer stories
        are run, and False if the whole benchmark should be run.
      repeat: number of times to repeat the entire story set.
        can be None, which defaults to the benchmark default pageset_repeat.
    """
    self.benchmark = benchmark
    super().__init__(benchmark.Name(), self.get_repeat(repeat))
    self.abridged: Final[bool] = abridged
    self._stories: Optional[tuple[str, ...]] = None
    self._exhaustive_stories: Optional[tuple[str, ...]] = None

  def get_repeat(self, repeat: Optional[int]) -> int:
    if repeat is not None:
      return repeat
    return self.benchmark.options.get('pageset_repeat', 1)

  @property
  def stories(self) -> tuple[str, ...]:
    if self._stories is not None:
      return self._stories
    story_set = benchmark_utils.GetBenchmarkStorySet(self.benchmark())
    abridged_story_set_tag = (story_set.GetAbridgedStorySetTagFilter()
                              if self.abridged else None)
    story_filter_obj = story_filter.StoryFilter(
        abridged_story_set_tag=abridged_story_set_tag)
    stories = story_filter_obj.FilterStories(story_set)
    self._stories = tuple(story.name for story in stories)
    return self._stories

  @property
  def exhaustive_stories(self) -> tuple[str, ...]:
    if self._exhaustive_stories is not None:
      return self._exhaustive_stories
    story_set = benchmark_utils.GetBenchmarkStorySet(self.benchmark(),
                                                     exhaustive=True)
    abridged_story_set_tag = (story_set.GetAbridgedStorySetTagFilter()
                              if self.abridged else None)
    story_filter_obj = story_filter.StoryFilter(
        abridged_story_set_tag=abridged_story_set_tag)
    stories = story_filter_obj.FilterStories(story_set)
    self._exhaustive_stories = tuple(story.name for story in stories)
    return self._exhaustive_stories


class ExecutableConfig(BenchmarkConfig):

  def __init__(self,
               name: str,
               path: Optional[str] = None,
               flags: tuple[str, ...] = (),
               estimated_runtime: int = 60,
               repeat: int = 1):
    super().__init__(name, repeat, flags)
    self.path: Final[str] = path or name
    self.estimated_runtime: Final[int] = estimated_runtime
    self.abridged: Final[bool] = False
    self.stories: Final[tuple[str, ...]] = (GTEST_STORY_NAME, )


class CrossbenchConfig(BenchmarkConfig):
  def __init__(self,
               name: str,
               crossbench_name: str,
               estimated_runtime: int = 60,
               stories: Optional[tuple[str]] = None,
               flags: tuple[str, ...] = (),
               repeat: int = 1,
               auto_enable_field_trials: bool = True):
    flags = self._process_flags(flags, auto_enable_field_trials)
    super().__init__(name, repeat, flags)
    self.crossbench_name: Final[str] = crossbench_name
    self.estimated_runtime: Final[int] = estimated_runtime
    self.stories: Final[tuple[str, ...]] = stories or ('default', )

  def _process_flags(self, flags: tuple[str, ...],
                     auto_enable_field_trials: bool) -> tuple[str, ...]:
    if auto_enable_field_trials:
      # Somewhat hacky solution if we want to run without field trials.
      if "--disable-field-trials" not in flags and (
          "--disable-field-trial-config" not in flags):
        flags += ("--enable-field-trials", )
    assert len(flags) == len(
        set(flags)), (f"Found duplicate arguments in {flags}")
    return flags


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
                            flags: tuple[str, ...] = ()):
  return ExecutableConfig('sync_performance_tests',
                          path=path,
                          flags=('--test-launcher-jobs=1',
                                 '--test-launcher-retry-limit=0', *flags),
                          estimated_runtime=estimated_runtime)


@_register('base_perftests')
def _base_perftests(estimated_runtime: int = 270,
                    path=None,
                    flags: tuple[str, ...] = ()):
  return ExecutableConfig('base_perftests',
                          path=path,
                          flags=('--test-launcher-jobs=1',
                                 '--test-launcher-retry-limit=0', *flags),
                          estimated_runtime=estimated_runtime)


@_register('components_perftests')
def _components_perftests(estimated_runtime: int = 110,
                          flags: tuple[str, ...] = ()):
  return ExecutableConfig('components_perftests',
                          flags=('--xvfb', *flags),
                          estimated_runtime=estimated_runtime)


@_register('dawn_perf_tests')
def _dawn_perf_tests(estimated_runtime: int = 270, flags: tuple[str, ...] = ()):
  return ExecutableConfig('dawn_perf_tests',
                          flags=('--test-launcher-jobs=1',
                                 '--test-launcher-retry-limit=0', *flags),
                          estimated_runtime=estimated_runtime)


@_register('tint_benchmark')
def _tint_benchmark(estimated_runtime: int = 180, flags: tuple[str, ...] = ()):
  return ExecutableConfig('tint_benchmark',
                          flags=('--use-chrome-perf-format', *flags),
                          estimated_runtime=estimated_runtime)


@_register('load_library_perf_tests')
def _load_library_perf_tests(estimated_runtime: int = 3,
                             flags: tuple[str, ...] = ()):
  return ExecutableConfig('load_library_perf_tests',
                          flags=flags,
                          estimated_runtime=estimated_runtime)


@_register('performance_browser_tests')
def _performance_browser_tests(estimated_runtime: int = 67,
                               flags: tuple[str, ...] = ()):
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
          *flags),
      estimated_runtime=estimated_runtime)


@_register('tracing_perftests')
def _tracing_perftests(estimated_runtime: int = 5, flags: tuple[str, ...] = ()):
  return ExecutableConfig('tracing_perftests',
                          flags=flags,
                          estimated_runtime=estimated_runtime)


@_register('views_perftests')
def _views_perftests(estimated_runtime: int = 7, flags: tuple[str, ...] = ()):
  return ExecutableConfig('views_perftests',
                          flags=('--xvfb', *flags),
                          estimated_runtime=estimated_runtime)


@_register('web_tests_cuj')
def _web_tests_cuj(estimated_runtime: int = 10, flags: tuple[str, ...] = ()):
  return CrossbenchConfig('web_tests_cuj',
                          'speedometer_3.1',
                          estimated_runtime=estimated_runtime,
                          flags=('--web-tests-cuj', '--debug', *flags))

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


@_register("speedometer3-turbolev_future.crossbench")
def _speedometer3_turbolev_future_crossbench(estimated_runtime: int = 60,
                                             flags: tuple[str, ...] = ()):
  flags += ("--js-flags=--turbolev-future", )
  return CrossbenchConfig(
      "speedometer3-turbolev_future.crossbench",
      "speedometer_3",
      estimated_runtime=estimated_runtime,
      flags=flags,
  )

@_register('browser_startup.crossbench')
def _browser_startup_crossbench(estimated_runtime: int = 60,
                                flags: tuple[str, ...] = ()):
  """Browser startup benchmark for InitialWebUI vs Baseline."""
  # We cannot use --browser-config here because it conflicts with the
  # --browser flag automatically added by the Chromium test runner.

  # NOTE: Keep this list in sync with:
  # third_party/crossbench/config/benchmark/browser_startup/browser.config.hjson
  INITIAL_WEBUI_FEATURES = (
      "InitialWebUI:high_stream_priority/true,"
      "WebUIReloadButton:WebUIReloadButtonDeferBrowserViewShow/true/"
      "WebUIReloadButtonKeepVisibleUntilPaint/true/"
      "WebUIReloadButtonPrewarmWebUI/true,"
      "SkipIPCChannelPausingForNonGuests,WebUIInProcessResourceLoadingV2,"
      "InitialWebUISyncNavStartToCommit,InitialWebUIWithoutExtensions,"
      "WebUIBundledCodeCache,SendGPUChannelEarly"
  )
  return CrossbenchConfig('browser_startup.crossbench',
                          'browser-startup',
                          estimated_runtime=estimated_runtime,
                          flags=(f'--enable-features={INITIAL_WEBUI_FEATURES}',
                                 *flags))


@_register('speedometer_main.crossbench')
def _speedometer_main_crossbench(estimated_runtime: int = 60,
                                 flags: tuple[str, ...] = ()):
  # The latest WIP speedometer version
  return CrossbenchConfig('speedometer_main.crossbench',
                          'speedometer_main',
                          estimated_runtime=estimated_runtime,
                          flags=('--detailed-metrics', *flags))


@_register('speedometer3.a11y.crossbench')
def _speedometer3_a11y_crossbench(estimated_runtime: int = 60,
                                  flags: tuple[str, ...] = ()):
  """Latest Speedometer 3 with accessibility flag enabled."""
  return CrossbenchConfig(
      'speedometer3.a11y.crossbench',
      'speedometer_3',
      estimated_runtime=estimated_runtime,
      flags=('--extra-browser-args=--force-renderer-accessibility', *flags))


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
                          flags=flags,
                          auto_enable_field_trials=False)


@_register('embedder.crossbench')
def _crossbench_embedder(estimated_runtime: int = 900,
                         flags: tuple[str, ...] = ()):
  return CrossbenchConfig('embedder.crossbench',
                          'embedder',
                          estimated_runtime=estimated_runtime,
                          flags=flags,
                          auto_enable_field_trials=False)


@_register('gma.embedder.crossbench')
def _crossbench_gma_embedder(estimated_runtime: int = 900,
                            flags: tuple[str, ...] = ()):
  return CrossbenchConfig('gma.embedder.crossbench',
                          'embedder',
                          estimated_runtime=estimated_runtime,
                          flags=flags,
                          auto_enable_field_trials=False)


@_register('shell.embedder.benchmark')
def _crossbench_shell_embedder(estimated_runtime: int = 900,
                            flags: tuple[str, ...] = ()):
  return CrossbenchConfig('shell.embedder.benchmark',
                          'embedder',
                          estimated_runtime=estimated_runtime,
                          flags=flags,
                          auto_enable_field_trials=False)


@_register('devtools_frontend.crossbench')
def _devtools_frontend_crossbench(estimated_runtime: int = 60,
                                  flags: tuple[str, ...] = ()):
  return CrossbenchConfig('devtools_frontend.crossbench',
                          'devtools_frontend',
                          estimated_runtime=estimated_runtime,
                          flags=flags)


@_register('blink-ai.crossbench')
def _crossbench_blink_ai(estimated_runtime: int = 300,
                         flags: tuple[str, ...] = ()):
  return CrossbenchConfig('blink-ai.crossbench',
                          'blink-ai',
                          estimated_runtime=estimated_runtime,
                          stories=('language_model', ),
                          flags=flags,
                          auto_enable_field_trials=False)


PLATFORM_INFO = {
    'linux-perf': {
        'description': ('Ubuntu-22.04, Precision 3930 Rack, '
                        'NVIDIA GeForce GTX 1660'),
        'num_shards':
        4,
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
    'mac-m4-pro-perf': {
        'description': 'MacBook Pro M4 ARM',
        'num_shards': 15,
        'platform_os': 'mac',
        'is_fyi': False
    },
    'mac-m5-pro-perf': {
        'description': 'Mac M5 PRO ARM',
        'num_shards': 2,
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
        'num_shards': 2,
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
        'num_shards': 38,
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
        'num_shards': 19,
        'platform_os': 'android',
        'is_fyi': False
    },
    'android-pixel4_webview-perf-pgo': {
        'description': 'Android R',
        'num_shards': 12,
        'platform_os': 'android',
        'is_fyi': False
    },
    'android-pixel6-perf-pgo': {
        'description': 'Android U',
        'num_shards': 8,
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
        'num_shards': 8,
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
    'android-pixel10-perf': {
        'description': 'Android B',
        'num_shards': 25,
        'platform_os': 'android',
        'is_fyi': False
    },
    'android-pixel10_webview-perf': {
        'description': 'Android B',
        'num_shards': 23,
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

# TODO: add more details.
BENCHMARK_INFO = {k: {} for k, v in _BENCHMARKS_CONFIG_FACTORIES.items()}

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
  # pylint: disable=comparison-with-callable)
  is_telemetry = (factory == _TelemetryConfig)
  # pylint: enable=comparison-with-callable)
  reader = ReadCSV(file_path)
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


def ReadCSV(file_path: pathlib.Path):
  contents = file_path.read_text(encoding='utf-8')
  reader = csv.DictReader(_StripComments(io.StringIO(contents)),
                          restkey='*flag')
  return reader


def _StripComments(iterator):
  for line in iterator:
    if line and line[0] != '#':
      yield line

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

ALL_PLATFORMS: set[_PerfPlatform] = set()
ALL_PLATFORMS = LoadAllScheduleFiles()

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
