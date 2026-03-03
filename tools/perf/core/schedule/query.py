#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import argparse
import csv
import shutil
import pathlib
import sys
import tempfile
from collections import defaultdict

# Add tools/perf to sys.path
FILE_PATH = pathlib.Path(__file__).resolve()
sys.path.append(str(FILE_PATH.parents[2]))

from core import path_util

path_util.AddTelemetryToPath()

from core import bot_platforms


def _get_schedule_files(directory: pathlib.Path) -> list[pathlib.Path]:
  return sorted(directory.glob('*.csv'))


def load_benchmark(path: pathlib.Path) -> list[dict[str, str]]:
  return list(bot_platforms.ReadCSV(path))


def cmd_list(args):
  directory = pathlib.Path(__file__).resolve().parent
  schedule_files = _get_schedule_files(directory)

  if bot := args.bot:
    print(f'Benchmarks for bot {bot!r}:')
    for schedule_file in schedule_files:
      for row in load_benchmark(schedule_file):
        if row['bot'] == bot:
          config = _format_bot_row(row)
          print(f'  {schedule_file.stem:40} ({config})')
  elif pattern := args.benchmark:
    for schedule_file in schedule_files:
      if not (schedule_file.match(pattern) or schedule_file.stem == pattern):
        continue
      print(f'Bots for benchmark {schedule_file.stem!r}:')
      for row in load_benchmark(schedule_file):
        config = _format_bot_row(row)
        print(f"  {row['bot']:40} ({config})")
  else:
    # list for all bots
    bot_to_benchmarks = defaultdict(list)
    for schedule_file in schedule_files:
      for row in load_benchmark(schedule_file):
        bot_to_benchmarks[row['bot']].append(
            (schedule_file.stem, _format_bot_row(row)))

    for bot in sorted(bot_to_benchmarks.keys()):
      print(f'Benchmarks for bot {bot!r}:')
      for benchmark, config in bot_to_benchmarks[bot]:
        print(f'  {benchmark:40} ({config})')


def _format_bot_row(row) -> str:
  return ', '.join(f'{k}={v}' for k, v in row.items() if k != 'bot' and v)


def _get_filtered_schedule_files(benchmarks: list[str]) -> list[pathlib.Path]:
  directory = pathlib.Path(__file__).resolve().parent
  csv_files = _get_schedule_files(directory)
  filtered_files = [
      csv_file for csv_file in csv_files if any(
          csv_file.match(pattern) or csv_file.stem == pattern
          for pattern in benchmarks)
  ]
  return filtered_files


def cmd_add(args):
  csv_files = _get_filtered_schedule_files(args.benchmarks)
  if not csv_files:
    print('No matching benchmarks found.')
    return
  for csv_file in csv_files:
    add_bot(csv_file, args.bot, args.repeat, args.shard, args.flags)


def add_bot(path: pathlib.Path, bot: str, repeat: int | None, shard: int | None,
            flags: str | None):
  data = load_benchmark(path)
  reader = bot_platforms.ReadCSV(path)
  fieldnames = list(reader.fieldnames or [])

  if any(row['bot'] == bot for row in data):
    print(f'Error: Bot {bot!r} already exists in {path.name}. '
          'Updates are not supported to preserve comments.')
    return

  new_row = {
      'bot': bot,
      'repeat': '1',
      'shard': '1',
  }
  if repeat:
    new_row['repeat'] = str(repeat)
  if shard:
    new_row['shard'] = str(shard)
  if flags is not None:
    if 'flags' not in fieldnames:
      print(f'Error: "flags" column missing in {path.name}. '
            'Cannot add flags without overwriting the header. '
            'Please add the column manually first.')
      return
    new_row['flags'] = flags

  with tempfile.TemporaryDirectory() as tmp_dir:
    temp_path = pathlib.Path(tmp_dir) / path.name
    shutil.copy(path, temp_path)
    with temp_path.open('a', newline='', encoding='utf-8') as f:
      writer = csv.DictWriter(f, fieldnames=fieldnames, lineterminator='\n')
      writer.writerow(new_row)
    # validation of the temp file.
    bot_platforms.LoadScheduleFile(temp_path, {})
    # If validation passes, replace the original
    shutil.move(temp_path, path)
  print(f'Added {bot} to {path.name}')


def cmd_remove(args):
  csv_files = _get_filtered_schedule_files(args.benchmakrs)
  if not csv_files:
    print('No matching benchmarks found.')
    return

  for csv_file in csv_files:
    remove_bot(csv_file, args.bot)


def remove_bot(path: pathlib.Path, bot: str):
  lines = path.read_text(encoding='utf-8').splitlines(keepends=True)
  if filtered_rows := _filter_bot_rows(lines, bot):
    path.write_text(''.join(filtered_rows), encoding='utf-8')
    print(f'Removed {bot} from {path.name}')


def _filter_bot_rows(rows: list[str], bot: str) -> list[str]:
  assert ',' not in bot, 'Invalid bot name {bot!r}'
  bot_prefix = bot + ','
  filtered_rows: list[str] = []
  removed = False
  previous_row = ''
  for row in rows:
    if not row.startswith(bot_prefix):
      filtered_rows.append(row)
      previous_row = row
    else:
      removed = True
      if previous_row.startswith('#'):
        filtered_rows.pop()
      previous_row = ''
  if removed:
    return filtered_rows
  return []


def main():
  parser = argparse.ArgumentParser(
      description='Query and modify benchmark schedules.')
  subparsers = parser.add_subparsers(dest='command')

  # list command
  list_parser = subparsers.add_parser('list', help='list benchmarks or bots')
  list_parser.add_argument('--bot', help='list all benchmarks run by this bot')
  list_parser.add_argument(
      '--benchmark',
      help='list all bots running this benchmark (supports globs)')

  # Add command
  add_parser = subparsers.add_parser('add',
                                     help='Add or update a bot in benchmarks')
  add_parser.add_argument('bot', help='Bot name')
  add_parser.add_argument('benchmarks',
                          nargs='+',
                          help='Benchmark names or glob patterns')
  add_parser.add_argument('--repeat', type=int, help='Number of repeats')
  add_parser.add_argument('--shard', type=int, help='Number of shards')
  add_parser.add_argument('--flags', help='Custom flags')

  # Remove command
  remove_parser = subparsers.add_parser('remove',
                                        help='Remove a bot from benchmarks')
  remove_parser.add_argument('bot', help='Bot name')
  remove_parser.add_argument('benchmarks',
                             nargs='+',
                             help='Benchmark names or glob patterns')

  args = parser.parse_args()
  cmd = args.command
  if cmd == 'list':
    cmd_list(args)
  elif cmd == 'add':
    cmd_add(args)
  elif cmd == 'remove':
    cmd_remove(args)
  else:
    parser.print_help()


if __name__ == '__main__':
  main()
