# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import tempfile
import unittest

from core import path_util

path_util.AddTelemetryToPath()

from core import bot_platforms  # pylint: disable=wrong-import-position
import generate_perf_sharding  # pylint: disable=wrong-import-position

_GTEST = bot_platforms.GTEST_STORY_NAME


class _FakeTelemetryConfig:
  def __init__(self, name, stories):
    self.name = name
    self.exhaustive_stories = tuple(stories)


class _FakeBuilder:
  def __init__(
    self,
    executables=(),
    crossbench=(),
    benchmark_configs=(),
    timing_file_path=None,
  ):
    self.executables = frozenset(executables)
    self.crossbench = frozenset(crossbench)
    self.benchmark_configs = frozenset(benchmark_configs)
    self.timing_file_path = timing_file_path


class MergeTimingDataTest(unittest.TestCase):
  def setUp(self):
    self.builder = _FakeBuilder(
      executables=[
        bot_platforms.ExecutableConfig(
          'dawn_perf_tests', estimated_runtime=600
        ),
        bot_platforms.ExecutableConfig('base_perftests', estimated_runtime=200),
      ],
      crossbench=[
        bot_platforms.CrossbenchConfig(
          'devtools_frontend.crossbench', 'devtools_frontend'
        ),
        bot_platforms.CrossbenchConfig(
          'two_story.crossbench', 'two_story', stories=('a', 'b')
        ),
        bot_platforms.CrossbenchConfig('unmeasured.crossbench', 'unmeasured'),
      ],
    )

  def _Merge(self, story_timing_data, run_total_timing_data):
    for entry in run_total_timing_data:
      entry.setdefault('num_runs', '20')
    merged = generate_perf_sharding.MergeTimingData(
      self.builder, story_timing_data, run_total_timing_data
    )
    return {e['name']: (float(e['duration']), e['source']) for e in merged}

  def testTelemetryStoriesKeepMeasuredAverage(self):
    merged = self._Merge(
      [{'name': 'system_health.memory_desktop/load:news', 'duration': '12.0'}],
      [],
    )
    self.assertEqual(
      merged['system_health.memory_desktop/load:news'], (12.0, 'measured')
    )

  def testExecutableUsesMeasuredRunTotal(self):
    merged = self._Merge(
      [{'name': 'dawn_perf_tests', 'duration': '2.0'}],
      [{'name': 'dawn_perf_tests', 'duration': '2295.0'}],
    )
    self.assertEqual(merged['dawn_perf_tests/' + _GTEST], (2295.0, 'measured'))
    # The bare per-result average must not leak into the timing data.
    self.assertNotIn('dawn_perf_tests', merged)

  def testExecutableWithoutDataFallsBackToEstimate(self):
    merged = self._Merge([], [])
    self.assertEqual(merged['dawn_perf_tests/' + _GTEST], (600.0, 'estimate'))
    self.assertEqual(merged['base_perftests/' + _GTEST], (200.0, 'estimate'))

  def testExecutableWithZeroDurationFallsBackToEstimate(self):
    merged = self._Merge([], [{'name': 'base_perftests', 'duration': '0'}])
    self.assertEqual(merged['base_perftests/' + _GTEST], (200.0, 'estimate'))

  def testTooFewRunsFallsBackToEstimate(self):
    merged = self._Merge(
      [], [{'name': 'base_perftests', 'duration': '50.0', 'num_runs': '9'}]
    )
    self.assertEqual(merged['base_perftests/' + _GTEST], (200.0, 'estimate'))

  def testMeasuredEntriesRecordNumRuns(self):
    merged = generate_perf_sharding.MergeTimingData(
      self.builder,
      [],
      [
        {'name': 'base_perftests', 'duration': '50.0', 'num_runs': '12'},
        {'name': 'two_story.crossbench', 'duration': '10.0', 'num_runs': '11'},
      ],
    )
    num_runs = {e['name']: e.get('num_runs') for e in merged}
    self.assertEqual(num_runs['base_perftests/' + _GTEST], 12)
    self.assertEqual(num_runs['two_story.crossbench/a'], 11)
    self.assertIsNone(num_runs['dawn_perf_tests/' + _GTEST])

  def testCrossbenchUsesMeasuredRunTotal(self):
    merged = self._Merge(
      [], [{'name': 'devtools_frontend.crossbench', 'duration': '876.0'}]
    )
    self.assertEqual(
      merged['devtools_frontend.crossbench/default'], (876.0, 'measured')
    )

  def testCrossbenchDurationSplitAcrossStories(self):
    merged = self._Merge(
      [], [{'name': 'two_story.crossbench', 'duration': '100.0'}]
    )
    self.assertEqual(merged['two_story.crossbench/a'], (50.0, 'measured'))
    self.assertEqual(merged['two_story.crossbench/b'], (50.0, 'measured'))

  def testCrossbenchWithoutDataIsLeftOut(self):
    merged = self._Merge([], [])
    self.assertFalse(
      [name for name in merged if name.startswith('unmeasured.crossbench')]
    )

  def testUnknownBareNamesAreIgnoredInRunTotals(self):
    merged = self._Merge([], [{'name': 'other_tests', 'duration': '5.0'}])
    self.assertNotIn('other_tests', merged)


class FilterTimingDataTest(unittest.TestCase):
  def testKeepsTelemetryExecutableAndCrossbenchEntries(self):
    with tempfile.TemporaryDirectory() as temp_dir:
      timing_file_path = os.path.join(temp_dir, 'timing.json')
      builder = _FakeBuilder(
        executables=[bot_platforms.ExecutableConfig('base_perftests')],
        crossbench=[
          bot_platforms.CrossbenchConfig(
            'speedometer3.crossbench', 'speedometer_3'
          )
        ],
        benchmark_configs=[_FakeTelemetryConfig('blink_perf.css', ['s_1'])],
        timing_file_path=timing_file_path,
      )
      timing_data = [
        {'name': 'blink_perf.css/s_1', 'duration': '1.0'},
        {'name': 'blink_perf.css/removed', 'duration': '1.0'},
        {'name': 'base_perftests/' + _GTEST, 'duration': '2.0'},
        {'name': 'speedometer3.crossbench/default', 'duration': '3.0'},
        {'name': 'descheduled.crossbench/default', 'duration': '4.0'},
      ]
      with open(timing_file_path, 'w') as f:
        json.dump(timing_data, f)

      generate_perf_sharding._FilterTimingData(builder)

      with open(timing_file_path) as f:
        filtered_names = [e['name'] for e in json.load(f)]
    self.assertEqual(
      filtered_names,
      [
        'blink_perf.css/s_1',
        'base_perftests/' + _GTEST,
        'speedometer3.crossbench/default',
      ],
    )


if __name__ == '__main__':
  unittest.main()
