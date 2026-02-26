# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import csv
import sys
from pathlib import Path

# Add tools/perf to sys.path
FILE_PATH = Path(__file__).resolve()
sys.path.append(str(FILE_PATH.parents[1]))

from core import path_util

path_util.AddTelemetryToPath()

from core import bot_platforms
from core import dump_bot_platforms



class ScheduleValidationTest(unittest.TestCase):

  def testScheduleFiles(self):
    schedule_dir = FILE_PATH.absolute().parent / 'schedule'
    if not schedule_dir.is_dir():
      self.fail("schedule/ directory not found. Run parse_schedule.py first.")
    csv_files = schedule_dir.glob("*.csv")
    if not csv_files:
      self.fail("No CSV files found in schedule/ directory")

    for path in csv_files:
      with self.subTest(path=path):
        with path.open('r') as f:
          reader = csv.DictReader(f)
          self.verify_schedule(path, reader)

  def verify_schedule(self, path, reader):
    if 'flags' in reader.fieldnames:
      self.assertEqual(reader.fieldnames[-1], 'flags',
                       f"'flags' must be the last column in {path}")
    bots = set()
    row_count = 0
    for row in reader:
      row_count += 1
      bot = row.get('bot')
      # Validate bot uniqueness
      self.assertIsNotNone(bot, f"Missing 'bot' column in {path}")
      self.assertIn(
          bot, bot_platforms.PLATFORM_INFO.keys(),
          f"Bot '{bot}' in {path.name} not found in "
          f"bot_platforms.ALL_PLATFORM_NAMES")
      self.assertNotIn(bot, bots, f"Duplicate bot '{bot}' in {path}")
      bots.add(bot)
      repeats = row.get('repeat')
      self.assertIsNotNone(repeats, f"Missing 'repeat' column in {path}")
      repeats = int(repeats)
      self.assertGreater(
          repeats, 0, f"'repeats' value {repeats} must be positive in {path} "
          f"for bot {bot}")
      shard = row.get('shard')
      self.assertIsNotNone(shard, f"Missing 'shard' column in {path}")
      shard = int(shard)
      self.assertGreater(
          shard, 0, f"'shard' value {shard} must be positive in {path} "
          f"for bot {bot}")
    self.assertGreater(row_count, 0,
                       f"No rows found in {path}, please remove file.")

  def testParse(self):
    schedule_dir = FILE_PATH.absolute().parent / 'schedule'
    csv_files = schedule_dir.glob("*.csv")
    for path in csv_files:
      with self.subTest(path=path):
        configs = {}
        # This will trigger the assertion in bot_platforms.LoadScheduleFile
        # if there's a duplicate bot.
        try:
          bot_platforms.LoadScheduleFile(path, configs)
        except AssertionError as e:
          self.fail(f"Validation failed for {path}: {e}")
        self.assertTrue(configs)

  def assert_configs_equal(self, legacy_configs, csv_configs, config_type, bot):
    legacy_map = {c['name']: c for c in legacy_configs}
    csv_map = {c['name']: c for c in csv_configs}

    legacy_keys = set(legacy_map.keys())
    csv_keys = set(csv_map.keys())

    if legacy_keys != csv_keys:
      missing_from_csv = sorted(list(legacy_keys - csv_keys))
      extra_in_csv = sorted(list(csv_keys - legacy_keys))
      msg = f"\n{config_type} mismatch for bot '{bot}':\n"
      if missing_from_csv:
        msg += f"  Missing from CSV: {missing_from_csv}\n"
      if extra_in_csv:
        msg += f"  Extra in CSV: {extra_in_csv}\n"
      self.fail(msg)

    for name in sorted(list(legacy_keys)):
      with self.subTest(benchmark=name):
        self.assertEqual(legacy_map[name], csv_map[name])

  def testCompareSchedules(self):
    # TODO: remove once migration is complete.
    legacy_platforms_list = bot_platforms.CreateLegacySchedule()
    legacy_bots = {p.name: p for p in legacy_platforms_list}
    csv_platforms_set = bot_platforms.LoadAllScheduleFiles()
    csv_bots = {p.name: p for p in csv_platforms_set}

    self.assertEqual(set(legacy_bots.keys()), set(csv_bots.keys()),
                     "Bot list mismatch")

    for bot, legacy_platform in legacy_bots.items():
      with self.subTest(bot=bot):
        csv_platform = csv_bots[bot]

        # Compare benchmark_configs
        legacy_benchmarks = [
            dump_bot_platforms.serialize_benchmark_config(b)
            for b in legacy_platform.benchmark_configs
        ]
        csv_benchmarks = [
            dump_bot_platforms.serialize_benchmark_config(b)
            for b in csv_platform.benchmark_configs
        ]
        self.assert_configs_equal(legacy_benchmarks, csv_benchmarks,
                                  "Benchmark configs", bot)

        # Compare executables
        legacy_executables = [
            dump_bot_platforms.serialize_executable_config(e)
            for e in legacy_platform.executables
        ]
        csv_executables = [
            dump_bot_platforms.serialize_executable_config(e)
            for e in csv_platform.executables
        ]
        self.assert_configs_equal(legacy_executables, csv_executables,
                                  "Executable configs", bot)

        # Compare crossbench
        legacy_crossbench = [
            dump_bot_platforms.serialize_crossbench_config(c)
            for c in legacy_platform.crossbench
        ]
        csv_crossbench = [
            dump_bot_platforms.serialize_crossbench_config(c)
            for c in csv_platform.crossbench
        ]
        self.assert_configs_equal(legacy_crossbench, csv_crossbench,
                                  "Crossbench configs", bot)


if __name__ == '__main__':
  unittest.main()
