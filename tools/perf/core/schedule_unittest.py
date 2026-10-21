# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import sys
import tempfile
from pathlib import Path

# Add tools/perf to sys.path
FILE_PATH = Path(__file__).resolve()
sys.path.append(str(FILE_PATH.parents[1]))

from core import path_util

path_util.AddTelemetryToPath()

from core import bot_platforms


class ScheduleValidationTest(unittest.TestCase):
  def testInlineComments(self):
    with tempfile.TemporaryDirectory() as tmp_dir:
      benchmark_name = 'speedometer3.crossbench'
      csv_path = Path(tmp_dir) / f"{benchmark_name}.csv"
      csv_content = """bot,repeat,shard
# Full line comment
linux-perf,1,1 # Inline comment
# Another full line comment
win-10-perf,2,1
"""
      csv_path.write_text(csv_content, encoding='utf-8')

      configs = {}
      bot_platforms.LoadScheduleFile(csv_path, configs)
      self.assertIn('linux-perf', configs)
      self.assertIn('win-10-perf', configs)
      self.assertEqual(len(configs), 2)

  def testAllowedBenchmarks(self):
    with tempfile.TemporaryDirectory() as tmp_dir:
      disallowed_csv = Path(tmp_dir) / 'speedometer3.crossbench.csv'
      disallowed_csv.write_text(
        'bot,repeat,shard\nlinux-rtx4070-perf,1,1\n', encoding='utf-8'
      )
      with self.assertRaisesRegex(
        AssertionError, "Bot 'linux-rtx4070-perf' only allows benchmarks"
      ):
        bot_platforms.LoadScheduleFile(disallowed_csv, {})

      allowed_csv = Path(tmp_dir) / 'blink-ai.crossbench.csv'
      allowed_csv.write_text(
        'bot,repeat,shard\nlinux-rtx4070-perf,1,1\n', encoding='utf-8'
      )
      configs = {}
      bot_platforms.LoadScheduleFile(allowed_csv, configs)
      self.assertIn('linux-rtx4070-perf', configs)

  def testScheduleFiles(self):
    schedule_dir = FILE_PATH.absolute().parent / 'schedule'
    if not schedule_dir.is_dir():
      self.fail("schedule/ directory not found. Run parse_schedule.py first.")
    csv_files = schedule_dir.glob("*.csv")
    if not csv_files:
      self.fail("No CSV files found in schedule/ directory")

    for path in csv_files:
      with self.subTest(path=path):
        self.verify_schedule(path)

  def verify_schedule(self, path):
    content = path.read_text(encoding='utf-8')
    assert content, "Unexpected empty file"
    self.assertTrue(
      content.endswith('\n'), f"File {path.name} must end with a newline"
    )
    reader = bot_platforms.ReadCSV(path)
    assert reader.fieldnames, "Missing header"
    if 'flags' in reader.fieldnames:
      self.assertEqual(
        reader.fieldnames[-1],
        'flags',
        f"'flags' must be the last column in {path}",
      )
    bots = set()
    row_count = 0
    for row in reader:
      row_count += 1
      bot = row.get('bot')
      # Validate bot uniqueness
      self.assertIsNotNone(bot, f"Missing 'bot' column in {path}")
      self.assertIn(
        bot,
        bot_platforms.PLATFORM_INFO.keys(),
        f"Bot '{bot}' in {path.name} not found in "
        f"bot_platforms.ALL_PLATFORM_NAMES",
      )
      allowed_benchmarks = bot_platforms.PLATFORM_INFO[bot].get(
        'allowed_benchmarks'
      )
      if allowed_benchmarks is not None:
        self.assertIn(
          path.stem,
          allowed_benchmarks,
          f"Bot '{bot}' only allows benchmarks {sorted(allowed_benchmarks)}, "
          f"cannot schedule '{path.stem}' in {path.name}",
        )
      self.assertNotIn(bot, bots, f"Duplicate bot '{bot}' in {path}")
      bots.add(bot)
      repeats = row.get('repeat')
      self.assertIsNotNone(repeats, f"Missing 'repeat' column in {path}")
      if repeats:
        repeats = int(repeats)
        self.assertGreater(
          repeats,
          0,
          f"'repeats' value {repeats} must be positive in {path} for bot {bot}",
        )
      shard = row.get('shard')
      self.assertIsNotNone(shard, f"Missing 'shard' column in {path}")
      shard = int(shard)
      self.assertGreater(
        shard,
        0,
        f"'shard' value {shard} must be positive in {path} for bot {bot}",
      )
    self.assertGreater(
      row_count, 0, f"No rows found in {path}, please remove file."
    )

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


if __name__ == '__main__':
  unittest.main()
