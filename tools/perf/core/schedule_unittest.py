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
          bot, bot_platforms.ALL_PLATFORM_NAMES,
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


if __name__ == '__main__':
  unittest.main()
