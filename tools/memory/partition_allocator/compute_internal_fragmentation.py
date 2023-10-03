#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Parses allocation profiles from a trace and computes the internal
fragmentation from PartitionAlloc, given a list of buckets.

We compute this as the difference in memory usage between the given trace, and
that same trace if we were to only allocate memory into one of the given
buckets. This provides a rough estimate of the increased memory usage given
a specific bucket mapping. We probably underestimate the increase here, as this
script only considers surviving allocations.

NOTE: Buckets (given with --buckets flag) should be a subset of the buckets in
Chrome when the trace was recorded. To see why, say we have a request size of
999B, and the next bucket in Chrome is 1024B. The 999B allocation will be
recorded in the trace as 1024B (since we don't record the requested size in
most cases, just the allocated size). Say our buckets (from --buckets flag)
have a 1000B bucket. Our 999B allocation would have gone there, if we were
actually using that bucket mapping in Chrome, which would have reduced
fragmentation. However, this script would round this upwards, based on the
1024B size, and tell us we increased fragmentation instead.

This script outputs two sets of graphs. The first shows the increase in memory
usage that would occur for each bucket, using the given bucket mapping, in
percent. The second shows the increase in memory (in bytes) that would occur
from using the given bucket mapping compared with the one from the trace.

See: trace_utils.py for details on collecting a trace.
"""

import argparse
import logging
import os
import re

from matplotlib import pylab as plt
import numpy as np

import trace_utils


def _LoadBuckets(filename: str) -> list[int]:
  """Loads a list of bucket sizes from a file. The file should contain
  integers, in Newline Separated Value format.

  Args:
    filename: Filename.

  Returns:
    A sorted list of bucket sizes to use.
  """
  try:
    f = open(filename, 'r')
    buckets = [int(line) for line in f.readlines()]
  finally:
    if f is not None:
      f.close()

  buckets.sort()
  return buckets


def _RoundUpToNearestBucket(buckets: list[int], size: int) -> int:
  """Rounds a size up to the nearest bucket.

  Args:
    buckets: Sorted list of bucket sizes.
    size: The size to round upwards.

  Returns:
    The closest bucket size to |size|, rounding upwards.
  """
  curr = None
  for bucket_size in buckets:
    curr = bucket_size
    if size <= bucket_size:
      break
  assert curr is not None
  return curr


def _SummarizeStatsForAllocators(result_for_pid: dict, allocators: dict,
                                 buckets: list[int]):
  """We compute the wasted memory here by taking the 'allocated_objects_size'
  and rounding it up to nearest bucket (in |buckets|), and comparing the two.
  """
  size_counts = []
  total_allocated_size = 0
  rounded_allocated_size = 0
  # We are only interested in the main malloc partition, reported in
  # |ReportPartitionAllocStats|.
  pattern = re.compile("malloc/partitions/allocator/buckets/bucket_\d+")
  for entry in allocators:
    # |entry| can represent a subset of the allocations for a given allocator.
    if (not pattern.match(entry)):
      continue
    attrs = allocators[entry]['attrs']

    allocated_objects_size = trace_utils.GetAllocatorAttr(
        attrs, 'allocated_objects_size')
    slot_size = trace_utils.GetAllocatorAttr(attrs, 'slot_size')
    if allocated_objects_size != 0:
      # See: |CanStoreRawSize| in
      # base/allocator/partition_allocator/src/partition_alloc/partition_bucket.h
      # The single slot span size below assumes we have 4 KiB pages.
      if slot_size >= 0xE0000:  # Direct Mapped, so not affected by bucket size
        rounded_allocated_objects_size = allocated_objects_size
      elif slot_size >= 4 << 14:  # Single slot span
        # We care about the increase in all memory here, not just the increase
        # in allocated size. To deal with this, we round up the
        # allocated_objects_size.
        allocated_objects_size = slot_size
        rounded_allocated_objects_size = _RoundUpToNearestBucket(
            buckets, slot_size)
      else:
        bucket_size = _RoundUpToNearestBucket(buckets, slot_size)
        rounded_allocated_objects_size = (allocated_objects_size /
                                          slot_size) * bucket_size
        assert bucket_size in buckets
        assert slot_size <= bucket_size
      fragmentation = (1 -
                       allocated_objects_size / rounded_allocated_objects_size)
      total_allocated_size += allocated_objects_size
      rounded_allocated_size += rounded_allocated_objects_size
    else:
      bucket_size = 0
      rounded_allocated_objects_size = 0
      fragmentation = 0

    size_counts.append(
        (slot_size, fragmentation * 100,
         rounded_allocated_objects_size - allocated_objects_size))
  size_counts.sort()
  result_for_pid['regression'] = (rounded_allocated_size - total_allocated_size
                                  ) * 100 / total_allocated_size
  result_for_pid['data'] = np.array(size_counts,
                                    dtype=[('size', np.int),
                                           ('fragmentation', np.int),
                                           ('unused', np.int)])


def _PlotProcess(all_data: dict, pid: int, output_prefix: str):
  """Represents the allocation size distribution.

  Args:
    all_data: As returned by _ParseTrace().
    pid: PID to plot the data for.
    output_prefix: Prefix of the output file.
  """
  data = all_data[pid]
  logging.info('Plotting data for PID %d' % pid)

  fragmentation_title = ('Internal Fragmentation (%%) vs Size - %s - %s' %
                         (data['name'], data['labels']))
  fragmentation_output = ('%s_%s_fragmentation.png' % (output_prefix, pid))
  trace_utils.PlotProcessFragmentation(fragmentation_title, data,
                                       fragmentation_output)

  unused_title = (
      'Internal Unused Memory vs Size - %s - %s (%.2f%% Regression)' %
      (data['name'], data['labels'], data['regression']))
  unused_output = ('%s_%d_unused.png' % (output_prefix, pid))
  trace_utils.PlotProcessWaste(unused_title, data, unused_output)


def _CreateArgumentParser():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--trace',
      type=str,
      required=True,
      help='Path to a trace.json[.gz] with memory-infra enabled.')
  parser.add_argument('--output-dir',
                      type=str,
                      required=True,
                      help='Output directory for graphs.')
  parser.add_argument('--buckets',
                      type=str,
                      required=True,
                      help='Path to file containing bucket sizes.')
  return parser


def main():
  logging.basicConfig(level=logging.INFO)
  parser = _CreateArgumentParser()
  args = parser.parse_args()

  logging.info('Loading the trace')
  trace = trace_utils.LoadTrace(args.trace)
  buckets = _LoadBuckets(args.buckets)

  logging.info('Parsing the trace')
  stats_per_process = trace_utils.ParseTrace(
      trace, (lambda result_for_pid, allocators: _SummarizeStatsForAllocators(
          result_for_pid, allocators, buckets)))

  logging.info('Plotting the results')
  for pid in stats_per_process:
    if 'data' in stats_per_process[pid]:
      _PlotProcess(stats_per_process, pid,
                   os.path.join(args.output_dir, 'internal'))


if __name__ == '__main__':
  main()
