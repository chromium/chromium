#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Parses Mac vmmap output to group and count IOAccelerator allocations.

This tool helps profile GPU memory allocations by reading vmmap output
directly from a running process (via PID) or from a saved text file.
"""

import argparse
import logging
import re
import subprocess
import sys
from collections import Counter


def parse_size(size_str):
  """Parses a size string with units (K, M, G) into bytes."""
  match = re.match(r'([0-9.]+)([KMG]?)', size_str)
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


def parse_vmmap_lines(lines):
  """Parses lines of vmmap output and extracts IOAccelerator resident+swap.

  Only includes entries that have non-zero resident or swapped memory.
  """
  sizes = []
  io_accelerator_re = re.compile(
      r'\[\s*([0-9.a-zA-Z]+)\s+([0-9.a-zA-Z]+)\s+[0-9.a-zA-Z]+'
      r'\s+([0-9.a-zA-Z]+)\]')
  for line in lines:
    if 'IOAccelerator' in line:
      bracket_match = io_accelerator_re.search(line)
      if bracket_match:
        # Same names as the result headers from vmmap output.
        vsize_str, rsdnt_str, swap_str = bracket_match.groups()
        rsdnt = parse_size(rsdnt_str)
        swap = parse_size(swap_str)
        if rsdnt == 0 and swap == 0:
          continue
        sizes.append(rsdnt + swap)
  return sizes


def main():
  logging.basicConfig(level=logging.INFO,
                      stream=sys.stderr,
                      format='%(message)s')
  parser = argparse.ArgumentParser(
      description=
      'Parse Mac vmmap output to group and count IOAccelerator allocations.')
  group = parser.add_mutually_exclusive_group(required=True)
  group.add_argument('--pid',
                     type=int,
                     help='PID of the process to call vmmap on.')
  group.add_argument('--file',
                     type=str,
                     help='Path to a cached vmmap output file.')

  args = parser.parse_args()

  if args.file:
    try:
      with open(args.file, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    except FileNotFoundError:
      logging.error(f'Error: File \'{args.file}\' not found.')
      sys.exit(1)
  elif args.pid:
    try:
      logging.info(f'Running vmmap on PID {args.pid}...')
      output = subprocess.check_output(['vmmap', str(args.pid)],
                                       stderr=subprocess.STDOUT)
      lines = output.decode('utf-8', errors='replace').splitlines()
    except subprocess.CalledProcessError as e:
      logging.error('Error running vmmap: %s',
                    e.output.decode('utf-8', errors='replace'))
      sys.exit(1)

  sizes = parse_vmmap_lines(lines)

  if not sizes:
    logging.error(
        'No active (resident or swapped) IOAccelerator regions found.')
    return

  print(f'Total IOAccelerator entries: {len(sizes)}')
  print(f'Total memory size: {sum(sizes) / (1024*1024):.2f} MB')
  print('\nHistogram of total memory size per bucket:')

  counter = Counter(sizes)
  max_bucket_mem = max(size * count for size, count in counter.items())

  for size in sorted(counter.keys()):
    count = counter[size]
    bucket_mem = size * count
    bucket_mem_mb = bucket_mem / (1024 * 1024)
    size_kb = size / 1024

    bar_len = int(60 * bucket_mem / max_bucket_mem) if max_bucket_mem > 0 else 0
    bar = '*' * bar_len
    print(f'{size_kb:8.1f} KB | {count:4d} entries | {bucket_mem_mb:7.2f} MB '
          f'| {bar}')


if __name__ == '__main__':
  main()
