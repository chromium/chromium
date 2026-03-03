# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import pathlib
import sys
import tempfile
from unittest import mock

# Add tools/perf to sys.path
FILE_PATH = pathlib.Path(__file__).resolve()
sys.path.append(str(FILE_PATH.parents[2]))

from core.schedule import query


class QueryTest(unittest.TestCase):

  def setUp(self):
    self.tmp_dir = tempfile.TemporaryDirectory()
    self.path = pathlib.Path(self.tmp_dir.name)

  def tearDown(self):
    self.tmp_dir.cleanup()

  def test_add_bot_to_file(self):
    csv_path = self.path / 'benchmark.csv'
    csv_path.write_text('bot,repeat,shard\nbot1,1,1\n', encoding='utf-8')

    with mock.patch('core.bot_platforms.LoadScheduleFile'):
      query.add_bot(csv_path, 'bot2', 2, 1, None)

    content = csv_path.read_text(encoding='utf-8')
    self.assertIn('bot2,2,1\n', content)
    self.assertIn('bot1,1,1\n', content)

  def test_add_bot_to_file_with_flags(self):
    csv_path = self.path / 'benchmark.csv'
    csv_path.write_text('bot,repeat,shard,flags\nbot1,1,1,flag1\n',
                        encoding='utf-8')

    with mock.patch('core.bot_platforms.LoadScheduleFile'):
      query.add_bot(csv_path, 'bot2', 1, 1, 'flag2')

    content = csv_path.read_text(encoding='utf-8')
    self.assertIn('bot2,1,1,flag2', content)

  def test_add_bot_to_file_duplicate(self):
    csv_path = self.path / "benchmark.csv"
    csv_path.write_text("bot,repeat,shard\nbot1,1,1\n", encoding='utf-8')

    with mock.patch('builtins.print') as mock_print:
      query.add_bot(csv_path, 'bot1', 1, 1, None)
      print_args = mock_print.call_args[0][0]
      self.assertIn("'bot1'", print_args)
      self.assertIn("already exists", print_args)

  def test_remove_bot_from_file(self):
    csv_path = self.path / 'benchmark.csv'
    csv_content = '''bot,repeat,shard
bot0,1,1
# Bot 1 comment
bot1,1,1
# Bot 2 comment
bot2,1,1
'''
    csv_path.write_text(csv_content, encoding='utf-8')

    query.remove_bot(csv_path, 'bot1')

    content = csv_path.read_text(encoding='utf-8')
    self.assertIn('bot0,1,1', content)
    self.assertNotIn('# Bot 1 comment', content)
    self.assertNotIn('bot1,1,1', content)
    self.assertIn('# Bot 2 comment', content)
    self.assertIn('bot2,1,1', content)

  def test_remove_bot_from_file_no_comment(self):
    csv_path = self.path / 'benchmark.csv'
    csv_content = '''bot,repeat,shard
bot1,1,1
bot2,1,1
'''
    csv_path.write_text(csv_content, encoding='utf-8')

    query.remove_bot(csv_path, 'bot1')

    content = csv_path.read_text(encoding='utf-8')
    self.assertNotIn('bot1,1,1', content)
    self.assertIn('bot2,1,1', content)

  def test_remove_bot_from_file_not_found(self):
    csv_path = self.path / 'benchmark.csv'
    csv_content = 'bot,repeat,shard\nbot1,1,1\n'
    csv_path.write_text(csv_content, encoding='utf-8')

    query.remove_bot(csv_path, 'bot2')
    content = csv_path.read_text(encoding='utf-8')
    self.assertEqual(content, csv_content)


if __name__ == '__main__':
  unittest.main()
