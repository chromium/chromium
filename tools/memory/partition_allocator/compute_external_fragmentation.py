#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Parses allocation profiles from a trace and computes the external
fragmentation from PartitionAlloc

We compute this as the difference between the memory allocated and the total
amount of memory used by the allocator. (For example, a bucket may have empty
slot spans, in which case PartitionAlloc is using more memory it has
allocated.)

The output of this script is two sets of graphs. The first shows external
fragmentation (as a percentage) for each bucket. The second shows the actual
amount of memory wasted due to external fragmentation for each bucket.

See: trace_utils.py for details on collecting a trace.
"""

import argparse
import logging
import os
import re

from matplotlib import pylab as plt
import numpy as np

import trace_utils


def _SummarizeStatsForAllocators(result_for_pid: dict, allocators: dict):
  """We compute the wasted memory here by taking the difference of
  'allocated_objects_size' and 'size'. See
  MemoryDumpPartitionStatsDumper::PartitionDumpTotals in chrome for details on
  where these are collected.
  """
  size_counts = []
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
    try:
      # 'size' is only available in the dump if 'allocated_objects_size' is
      # non-zero.
      size = trace_utils.GetAllocatorAttr(attrs, 'size')
      fragmentation = 1 - allocated_objects_size / size
    except KeyError:
      assert allocated_objects_size == 0
      size = 0
      fragmentation = 0
    assert allocated_objects_size <= size

    size_counts.append(
        (slot_size, 100 * fragmentation, size - allocated_objects_size))
  size_counts.sort()
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

  fragmentation_title = ('External Fragmentation (%%) vs Size - %s - %s' %
                         (data['name'], data['labels']))
  fragmentation_output = ('%s_%s_fragmentation.png' % (output_prefix, pid))
  trace_utils.PlotProcessFragmentation(fragmentation_title, data,
                                       fragmentation_output)

  unused_title = ('External Unused Memory vs Size - %s - %s' %
                  (data['name'], data['labels']))
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
  return parser


def main():
  logging.basicConfig(level=logging.INFO)
  parser = _CreateArgumentParser()
  args = parser.parse_args()

  logging.info('Loading the trace')
  trace = trace_utils.LoadTrace(args.trace)

  logging.info('Parsing the trace')
  stats_per_process = trace_utils.ParseTrace(trace,
                                             _SummarizeStatsForAllocators)

  logging.info('Plotting the results')
  for pid in stats_per_process:
    if 'data' in stats_per_process[pid]:
      _PlotProcess(stats_per_process, pid,
                   os.path.join(args.output_dir, 'external'))


if __name__ == '__main__':
  main()
