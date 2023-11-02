#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Shows all objects sharing the same PartitionAlloc bucket."""

import argparse
import collections
import logging
import json
import os
import subprocess
import sys


def _BucketSizes(alignment: int) -> list[int]:
  """Returns the bucket sizes for a given alignment."""
  # Adapted from partition_alloc_constants.h.
  _ALIGNMENT = alignment
  _MIN_BUCKETED_ORDER = 5 if _ALIGNMENT == 16 else 4
  _MAX_BUCKETETD_ORDER = 20
  _NUM_BUCKETED_ORDERS = (_MAX_BUCKETETD_ORDER - _MIN_BUCKETED_ORDER) + 1
  _NUM_BUCKETS_PER_ORDER_BITS = 2
  _NUM_BUCKETS_PER_ORDER = 1 << _NUM_BUCKETS_PER_ORDER_BITS
  _SMALLEST_BUCKET = 1 << (_MIN_BUCKETED_ORDER - 1)

  sizes = []
  current_size = _SMALLEST_BUCKET
  current_increment = _SMALLEST_BUCKET >> _NUM_BUCKETS_PER_ORDER_BITS
  for i in range(_NUM_BUCKETED_ORDERS):
    for j in range(_NUM_BUCKETS_PER_ORDER):
      if current_size % _SMALLEST_BUCKET == 0:
        sizes.append(current_size)
      current_size += current_increment
    current_increment = current_increment << 1
  return sizes


def _ParseExecutable(build_directory: str) -> dict:
  """Parses chrome in |build_directory| and returns types grouped by size.

  Args:
    build_directory: build directory, with chrome inside.

  Returns:
    {size: int -> [str]} List of all objects grouped by size.
  """
  try:
    p = subprocess.Popen([
        'pahole', '--show_private_classes', '-s',
        os.path.join(build_directory, 'chrome')
    ],
                         stdout=subprocess.PIPE,
                         stderr=subprocess.DEVNULL)
  except OSError as e:
    logging.error('Cannot execute pahole, is it installed? %s', e)
    sys.exit(1)

  logging.info('Parsing chrome')
  result = collections.defaultdict(list)
  count = 0
  for line in p.stdout:
    fields = line.decode('utf-8').split('\t')
    size = int(fields[1])
    name = fields[0]
    result[size].append(name)
    count += 1
    if count % 10000 == 0:
      logging.info('Found %d types', count)

  logging.info('Done. Found %d types', count)
  return result


def _MapToBucketSizes(objects_per_size: dict, alignment: int) -> dict:
  """From a size -> [types] mapping, groups types by bucket size.


  Args:
    objects_per_size: As returned by _ParseExecutable()
    alignment: 8 or 16, required alignment on the target platform.


  Returns:
    {slot_size -> [str]}
 """
  sizes = _BucketSizes(alignment)
  size_objects = list(objects_per_size.items())
  size_objects.sort()
  result = collections.defaultdict(list)
  next_bucket_index = 0
  for (size, objects) in size_objects:
    while next_bucket_index < len(sizes) and size > sizes[next_bucket_index]:
      next_bucket_index += 1
    if next_bucket_index >= len(sizes):
      break
    assert size <= sizes[next_bucket_index], size
    result[sizes[next_bucket_index]] += objects
  return result


_CACHED_RESULTS_FILENAME = 'cached.json'


def _LoadCachedResults():
  with open(_CACHED_RESULTS_FILENAME, 'r') as f:
    parsed = json.load(f)
    objects_per_size = {}
    for key in parsed:
      objects_per_size[int(key)] = parsed[key]
  return objects_per_size


def _StoreCachedResults(data):
  with open(_CACHED_RESULTS_FILENAME, 'w') as f:
    json.dump(data, f)


def main():
  logging.basicConfig(level=logging.INFO)
  parser = argparse.ArgumentParser()
  parser.add_argument('--build-directory',
                      type=str,
                      required=True,
                      help='Build directory')
  parser.add_argument('--slot-size', type=int)
  parser.add_argument('--type', type=str)
  parser.add_argument('--store-cached-results', action='store_true')
  parser.add_argument('--use-cached-results', action='store_true')
  parser.add_argument('--alignment', type=int, default=16)

  args = parser.parse_args()

  objects_per_size = None
  if args.use_cached_results:
    objects_per_size = _LoadCachedResults()
  else:
    objects_per_size = _ParseExecutable(args.build_directory)

  objects_per_bucket = _MapToBucketSizes(objects_per_size, args.alignment)
  if args.store_cached_results:
    _StoreCachedResults(objects_per_size)

  assert args.slot_size or args.type, 'Must provide a slot size or object type'

  size = 0
  if args.slot_size:
    size = args.slot_size
  else:
    for object_size in objects_per_size:
      if args.type in objects_per_size[object_size]:
        size = object_size
        break
    else:
      assert 'Type %s not found', args.type
    logging.info('Slot Size of %s = %d', args.type, size)

  print('Bucket sizes: %s' %
        ' '.join([str(x) for x in _BucketSizes(args.alignment)]))
  print('Objects in bucket %d' % size)
  for name in objects_per_size[size]:
    print('\t' + name)


if __name__ == '__main__':
  main()
