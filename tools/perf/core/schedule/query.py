#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import argparse
import csv
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
  with path.open('r', newline='', encoding='utf-8') as f:
    return list(csv.DictReader(f))


def save_benchmark(path: pathlib.Path, data: list[dict[str, str]],
                   fieldnames: list[str]):
  if 'flags' in fieldnames:
    fieldnames.append(fieldnames.pop(fieldnames.index('flags')))

  with tempfile.TemporaryDirectory() as tmp_dir:
    temp_path = pathlib.Path(tmp_dir) / path.name
    with temp_path.open('w', newline='', encoding='utf-8') as f:
      writer = csv.DictWriter(f, fieldnames=fieldnames, lineterminator='\n')
      writer.writeheader()
      for row in data:
        writer.writerow({k: row.get(k, '') for k in fieldnames})

    # Validate the generated file before overwriting the original
    bot_platforms.LoadScheduleFile(temp_path, {})

    # If validation passes, move the temp file to the original path
    path.write_bytes(temp_path.read_bytes())


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


def cmd_add(args):
  directory = pathlib.Path(__file__).resolve().parent
  files = _get_schedule_files(directory)

  targets = [
      p for p in files if any(
          p.match(pat) or p.stem == pat for pat in args.benchmarks)
  ]
  if not targets:
    print('No matching benchmarks found.')
    return

  for path in targets:
    data = load_benchmark(path)
    with path.open('r', newline='', encoding='utf-8') as f:
      fieldnames = list(csv.DictReader(f).fieldnames)

    bot_row = next((r for r in data if r['bot'] == args.bot), None)
    if not bot_row:
      bot_row = {'bot': args.bot}
      data.append(bot_row)

    if repeat := args.repeat:
      bot_row['repeat'] = str(repeat)
    if shard := args.shard:
      bot_row['shard'] = str(shard)
    # Explicit None check since we also want to look for no flags:
    if args.flags is not None:
      if 'flags' not in fieldnames:
        fieldnames.append('flags')
      bot_row['flags'] = args.flags

    # Ensure default values for new rows
    bot_row.setdefault('repeat', '1')
    bot_row.setdefault('shard', '1')

    data.sort(key=lambda x: x['bot'])
    save_benchmark(path, data, fieldnames)
    print(f'Updated {path.name}')


def cmd_remove(args):
  directory = pathlib.Path(__file__).resolve().parent
  files = _get_schedule_files(directory)

  schedule_files = []
  for csv_file in files:
    for benchmark_name in args.benchmarks:
      if csv_file.match(benchmark_name) or csv_file.stem == benchmark_name:
        schedule_files.append(csv_file)
        break

  if not schedule_files:
    print('No matching benchmarks found.')
    return

  for schedule_file in schedule_files:
    data = load_benchmark(schedule_file)
    new_data = [row for row in data if row['bot'] != args.bot]
    if len(new_data) != len(data):
      with schedule_file.open('r', newline='', encoding='utf-8') as f:
        fieldnames = list(csv.DictReader(f).fieldnames)
      save_benchmark(schedule_file, new_data, fieldnames)
      print(f'Removed {args.bot} from {schedule_file.name}')


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
