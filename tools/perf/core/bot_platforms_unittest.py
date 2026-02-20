# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import re
import tempfile
import pathlib
import sys

from pathlib import Path

# Add tools/perf to sys.path
FILE_PATH = Path(__file__).resolve()
sys.path.append(str(FILE_PATH.parents[1]))

from core import path_util

path_util.AddTelemetryToPath()

from core import bot_platforms

_SAFE_FILENAME_RE = re.compile(r'[^a-z0-9._-]')


class BotPlatformTest(unittest.TestCase):

  def testLoadScheduleFile(self):
    with tempfile.TemporaryDirectory() as tmpdir:
      tmpdir_path = pathlib.Path(tmpdir)
      # Use a known benchmark name that exists in
      # bot_platforms._BENCHMARKS_CONFIG
      file_path = tmpdir_path / 'speedometer3.crossbench.csv'
      file_path.write_text('bot,repeat,shard\n'
                           'test-bot,2,1\n',
                           encoding='utf-8')

      configs = {}
      bot_platforms.LoadScheduleFile(file_path, configs)

      self.assertIn('test-bot', configs)
      self.assertEqual(len(configs['test-bot']), 1)
      config = configs['test-bot'][0]
      self.assertEqual(config.name, 'speedometer3.crossbench')
      self.assertEqual(config.repeat, 2)

  def testLoadScheduleFileWithFlagsError(self):
    with tempfile.TemporaryDirectory() as tmpdir:
      tmpdir_path = pathlib.Path(tmpdir)
      file_path = tmpdir_path / 'speedometer3.crossbench.csv'
      file_path.write_text(
          'bot,flags\n'
          'test-bot-1,\'--extra-flag --other\'\n',
          encoding='utf-8')
      configs = {}
      with self.assertRaisesRegex(ValueError, "quote"):
        bot_platforms.LoadScheduleFile(file_path, configs)

  def testLoadScheduleFileWithFlags(self):
    with tempfile.TemporaryDirectory() as tmpdir:
      tmpdir_path = pathlib.Path(tmpdir)
      file_path = tmpdir_path / 'speedometer3.crossbench.csv'
      file_path.write_text(
          'bot,flags\n'
          'test-bot-1,--extra-flag --other\n'
          'test-bot-2,"--extra-flag --other"\n',
          encoding='utf-8')

      configs = {}
      bot_platforms.LoadScheduleFile(file_path, configs)

      for bot_configs in configs.values():
        self.assertEqual(len(bot_configs), 1)
        config = bot_configs[0]
        self.assertEqual(config.name, 'speedometer3.crossbench')
        self.assertSequenceEqual(config.arguments, ("--extra-flag", "--other"))

  def testLoadScheduleFileWithFlagValues(self):
    with tempfile.TemporaryDirectory() as tmpdir:
      tmpdir_path = pathlib.Path(tmpdir)
      file_path = tmpdir_path / 'speedometer3.crossbench.csv'
      file_path.write_text(
          'bot,flags\n'
          'test-bot-1,--extra-flag="1" --other="value"\n'
          'test-bot-2,--extra-flag=1 --other=value\n',
          encoding='utf-8')

      configs = {}
      bot_platforms.LoadScheduleFile(file_path, configs)

      for bot_configs in configs.values():
        self.assertEqual(len(bot_configs), 1)
        config = bot_configs[0]
        self.assertEqual(config.name, 'speedometer3.crossbench')
        self.assertSequenceEqual(config.arguments,
                                 ("--extra-flag=1", "--other=value"))

  def testLoadScheduleFileWithFlagValuesCommas(self):
    with tempfile.TemporaryDirectory() as tmpdir:
      tmpdir_path = pathlib.Path(tmpdir)
      file_path = tmpdir_path / 'speedometer3.crossbench.csv'
      file_path.write_text(
          'bot,flags\n'
          'test-bot-1,--extra-flag=1 --other=value '
          '--js-flags=--no-opt,--sparkplug\n'
          'test-bot-2,--extra-flag=1 --other=value '
          '--js-flags="--no-opt,--sparkplug"\n'
          'test-bot-3,"--extra-flag=1 --other=value '
          '--js-flags=--no-opt,--sparkplug"\n',
          encoding='utf-8')

      configs = {}
      bot_platforms.LoadScheduleFile(file_path, configs)

      for bot_configs in configs.values():
        self.assertEqual(len(bot_configs), 1)
        config = bot_configs[0]
        self.assertEqual(config.name, 'speedometer3.crossbench')
        self.assertSequenceEqual(config.arguments,
                                 ("--extra-flag=1", "--other=value",
                                  "--js-flags=--no-opt,--sparkplug"))

  def testLoadScheduleFileDuplicateBot(self):
    with tempfile.TemporaryDirectory() as tmpdir:
      tmpdir_path = pathlib.Path(tmpdir)
      file_path = tmpdir_path / 'speedometer3.crossbench.csv'
      file_path.write_text(
          'bot,repeat,shard\n'
          'test-bot,1,1\n'
          'test-bot,2,1\n',
          encoding='utf-8')

      configs = {}
      with self.assertRaisesRegex(AssertionError, "Duplicate bot 'test-bot'"):
        bot_platforms.LoadScheduleFile(file_path, configs)

  def testUniqueConfigNames(self):
    for platform in bot_platforms.ALL_PLATFORMS:
      seen_names = set()
      for config in platform.benchmark_configs:
        self.assertNotIn(
            config.name, seen_names,
            'Duplicate config name "%s" in platform "%s" (benchmark_config)' %
            (config.name, platform.name))
        seen_names.add(config.name)

      for config in platform.executables:
        self.assertNotIn(
            config.name, seen_names,
            'Duplicate config name "%s" in platform "%s" (executable)' %
            (config.name, platform.name))
        seen_names.add(config.name)

      for config in platform.crossbench:
        self.assertNotIn(
            config.name, seen_names,
            'Duplicate config name "%s" in platform "%s" (crossbench)' %
            (config.name, platform.name))
        seen_names.add(config.name)

  def testUniquePlatformNames(self):
    seen_names = set()
    for platform in bot_platforms.ALL_PLATFORMS:
      self.assertNotIn(platform.name, seen_names,
                       'Duplicate platform name "%s"' % platform.name)
      seen_names.add(platform.name)

  def testSafePlatformNames(self):
    for platform in bot_platforms.ALL_PLATFORMS:
      # Only allow lower-case names and conservatively safe file names.
      self.assertFalse(
          _SAFE_FILENAME_RE.search(platform.name)
          and ("HP-Candidate" not in platform.name),
          'Platform name "%s" contains unsafe characters for filenames' %
          platform.name)


if __name__ == '__main__':
  unittest.main()
