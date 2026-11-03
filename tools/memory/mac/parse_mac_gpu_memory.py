#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Parses Mac vmmap output to group IOAccelerator / IOSurface allocations.

This tool helps profile GPU memory allocations (including purgeable/volatile
state) by reading vmmap output directly from a running process (via PID) or
from a saved text file.
"""

import argparse
from collections import Counter, defaultdict
import dataclasses
import logging
import re
import subprocess
import sys


@dataclasses.dataclass
class RegionEntry:
  region_type: str
  vsize: int
  rsdnt: int
  dirty: int
  swap: int
  purge: str  # 'N' (non-volatile), 'V' (volatile), 'E' (empty), or '-'


@dataclasses.dataclass
class SummaryEntry:
  region_type: str
  virtual: int
  resident: int
  dirty: int
  swapped: int
  volatile: int
  nonvol: int
  empty: int
  count: int


def parse_size(size_str):
  """Parses a size string with units (B, K, M, G) into bytes."""
  match = re.match(r'^([0-9.]+)([BKMG]?)$', size_str.strip())
  if not match:
    return 0
  val, unit = match.groups()
  val = float(val)
  if unit == 'K':
    return int(val * 1024)
  elif unit == 'M':
    return int(val * 1024 * 1024)
  elif unit == 'G':
    return int(val * 1024 * 1024 * 1024)
  return int(val)


def format_mb(num_bytes):
  return f'{num_bytes / (1024 * 1024):7.2f} MB'


def parse_vmmap_lines(lines, region_types=('IOAccelerator', 'IOSurface')):
  """Parses lines of vmmap output for matching region types and summary rows.

  Excludes unmarked (PURGE=-) regions that have zero resident, dirty, and
  swapped memory.
  """
  entries = []
  summaries = []

  region_re = re.compile(
    r'^(?P<type>[A-Za-z0-9_ ()]+?)\s+'
    r'[0-9a-fA-F]+-[0-9a-fA-F]+\s+'
    r'\[\s*(?P<vsize>[0-9.BKMG]+)\s+'
    r'(?P<rsdnt>[0-9.BKMG]+)\s+'
    r'(?P<dirty>[0-9.BKMG]+)\s+'
    r'(?P<swap>[0-9.BKMG]+)\]\s+'
    r'\S+\s+\S+\s*'
    r'(?:PURGE=(?P<purge>[NVE]))?\s*'
    r'.*$'
  )

  summary_re = re.compile(
    r'^(?P<type>[A-Za-z0-9_ ()]+?)\s+'
    r'(?P<virtual>[0-9.BKMG]+)\s+'
    r'(?P<resident>[0-9.BKMG]+)\s+'
    r'(?P<dirty>[0-9.BKMG]+)\s+'
    r'(?P<swapped>[0-9.BKMG]+)\s+'
    r'(?P<volatile>[0-9.BKMG]+)\s+'
    r'(?P<nonvol>[0-9.BKMG]+)\s+'
    r'(?P<empty>[0-9.BKMG]+)\s+'
    r'(?P<count>[0-9]+)(?:\s+.*)?$'
  )

  in_summary = False
  for raw_line in lines:
    line = raw_line.rstrip('\n')
    if 'VIRTUAL' in line and 'VOLATILE' in line and 'NONVOL' in line:
      in_summary = True
      continue

    if in_summary:
      if (
        line.startswith('REGION TYPE')
        or line.startswith('===========')
        or not line.strip()
      ):
        continue
      if line.startswith('TOTAL'):
        in_summary = False
        continue
      sm = summary_re.match(line)
      if sm and any(t in sm.group('type') for t in region_types):
        summaries.append(
          SummaryEntry(
            region_type=sm.group('type').strip(),
            virtual=parse_size(sm.group('virtual')),
            resident=parse_size(sm.group('resident')),
            dirty=parse_size(sm.group('dirty')),
            swapped=parse_size(sm.group('swapped')),
            volatile=parse_size(sm.group('volatile')),
            nonvol=parse_size(sm.group('nonvol')),
            empty=parse_size(sm.group('empty')),
            count=int(sm.group('count')),
          )
        )
      continue

    if not any(t in line for t in region_types):
      continue
    m = region_re.match(line)
    if not m:
      continue
    rtype = m.group('type').strip()
    if not any(t in rtype for t in region_types):
      continue

    vsize = parse_size(m.group('vsize'))
    rsdnt = parse_size(m.group('rsdnt'))
    dirty = parse_size(m.group('dirty'))
    swap = parse_size(m.group('swap'))
    purge = m.group('purge') or '-'

    if rsdnt == 0 and dirty == 0 and swap == 0 and purge == '-':
      continue

    entries.append(
      RegionEntry(
        region_type=rtype,
        vsize=vsize,
        rsdnt=rsdnt,
        dirty=dirty,
        swap=swap,
        purge=purge,
      )
    )

  return entries, summaries


def main():
  logging.basicConfig(
    level=logging.INFO, stream=sys.stderr, format='%(message)s'
  )
  parser = argparse.ArgumentParser(
    description=(
      'Parse Mac vmmap output to group and count IOAccelerator and '
      'IOSurface allocations (including purgeable/volatile state).'
    )
  )
  group = parser.add_mutually_exclusive_group(required=True)
  group.add_argument(
    '--pid', type=int, help='PID of the process to call vmmap on.'
  )
  group.add_argument(
    '--file',
    type=str,
    help=(
      'Path to a cached vmmap output file '
      '(preferably from "vmmap -noCoalesce").'
    ),
  )
  parser.add_argument(
    '--types',
    nargs='+',
    default=['IOAccelerator', 'IOSurface'],
    help='Region types to include (default: IOAccelerator IOSurface).',
  )

  args = parser.parse_args()

  if args.file:
    try:
      with open(args.file, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    except FileNotFoundError:
      logging.error("Error: File '%s' not found.", args.file)
      sys.exit(1)
  elif args.pid:
    try:
      logging.info('Running vmmap -noCoalesce on PID %d...', args.pid)
      output = subprocess.check_output(
        ['vmmap', '-noCoalesce', str(args.pid)], stderr=subprocess.STDOUT
      )
      lines = output.decode('utf-8', errors='replace').splitlines()
    except subprocess.CalledProcessError as e:
      logging.error(
        'Error running vmmap: %s', e.output.decode('utf-8', errors='replace')
      )
      sys.exit(1)

  entries, summaries = parse_vmmap_lines(lines, tuple(args.types))

  if summaries:
    print('=== vmmap Summary Table ===')
    print(
      f'{"REGION TYPE":<26} | {"COUNT":>5} | {"VIRTUAL":>10} | '
      f'{"RESIDENT":>10} | {"DIRTY":>10} | {"SWAPPED":>10} | '
      f'{"VOLATILE":>10} | {"NONVOL":>10} | {"EMPTY":>10}'
    )
    print('-' * 123)
    for s in summaries:
      print(
        f'{s.region_type:<26} | {s.count:5d} | {format_mb(s.virtual)} | '
        f'{format_mb(s.resident)} | {format_mb(s.dirty)} | '
        f'{format_mb(s.swapped)} | {format_mb(s.volatile)} | '
        f'{format_mb(s.nonvol)} | {format_mb(s.empty)}'
      )
    print()

  if not entries:
    logging.error(
      'No active regions found for types: %s', ', '.join(args.types)
    )
    return

  print('=== Per-Region Purgeability Breakdown ===')
  # Each bucket stores: [count, vsize, rsdnt+swap, dirty].
  by_type_purge = defaultdict(lambda: [0, 0, 0, 0])
  for e in entries:
    bucket = by_type_purge[(e.region_type, e.purge)]
    bucket[0] += 1
    bucket[1] += e.vsize
    bucket[2] += e.rsdnt + e.swap
    bucket[3] += e.dirty

  purge_labels = {
    'N': 'N (Non-Volatile)',
    'V': 'V (Volatile/Purgeable)',
    'E': 'E (Empty/Purged)',
    '-': '- (Unmarked)',
  }
  print(
    f'{"REGION TYPE":<26} | {"PURGE STATE":<22} | {"COUNT":>5} | '
    f'{"VSIZE":>10} | {"RSDNT+SWAP":>10} | {"DIRTY":>10}'
  )
  print('-' * 98)
  for (rtype, purge), stats in sorted(by_type_purge.items()):
    count, vsize, rsdnt_swap, dirty = stats
    print(
      f'{rtype:<26} | {purge_labels.get(purge, purge):<22} | {count:5d} | '
      f'{format_mb(vsize)} | {format_mb(rsdnt_swap)} | {format_mb(dirty)}'
    )

  total_rsdnt_swap = sum(e.rsdnt + e.swap for e in entries)
  total_dirty = sum(e.dirty for e in entries)
  volatile_vsize = sum(e.vsize for e in entries if e.purge == 'V')
  empty_vsize = sum(e.vsize for e in entries if e.purge == 'E')
  nonvol_vsize = sum(e.vsize for e in entries if e.purge == 'N')
  print('-' * 98)
  print(
    f'Total entries: {len(entries)} | '
    f'Resident+Swap: {format_mb(total_rsdnt_swap).strip()} | '
    f'Dirty: {format_mb(total_dirty).strip()} | '
    f'Volatile VSIZE (V): {format_mb(volatile_vsize).strip()} | '
    f'Empty/Purged VSIZE (E): {format_mb(empty_vsize).strip()} | '
    f'Non-Volatile VSIZE (N): {format_mb(nonvol_vsize).strip()}'
  )

  entries_by_type = defaultdict(list)
  for e in entries:
    entries_by_type[e.region_type].append(e)

  for rtype, type_entries in sorted(entries_by_type.items()):
    print(f'\n=== Histogram of {rtype} Regions by Virtual Size Bucket ===')
    bucket_counts = Counter(e.vsize for e in type_entries)
    bucket_purge = defaultdict(Counter)
    for e in type_entries:
      bucket_purge[e.vsize][e.purge] += 1

    max_bucket_mem = max(size * count for size, count in bucket_counts.items())

    for size in sorted(bucket_counts.keys()):
      count = bucket_counts[size]
      bucket_mem = size * count
      bucket_mem_mb = bucket_mem / (1024 * 1024)
      size_kb = size / 1024
      p = bucket_purge[size]
      purge_str = f'N:{p["N"]:4d} V:{p["V"]:4d} E:{p["E"]:4d} -:{p["-"]:4d}'

      bar_len = (
        int(40 * bucket_mem / max_bucket_mem) if max_bucket_mem > 0 else 0
      )
      bar = '*' * bar_len
      print(
        f'{size_kb:8.1f} KB | {count:4d} entries ({purge_str}) | '
        f'{bucket_mem_mb:7.2f} MB | {bar}'
      )


if __name__ == '__main__':
  main()
