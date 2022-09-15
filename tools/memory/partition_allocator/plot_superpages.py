#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Maps the superpage occupancy, and the allocated object sizes on the heap.

To run this:

1. Build pa_dump_heap at a close-enough revision to the Chrome instance you're
   interested in.
2. Run pa_dump_heap --pid=PID_OF_RUNNING_CHROME_PROCESS --json=output.json
3. Run plot_superpages.py --json=output.json --output=output.png
"""

import argparse
import bisect
import math
import json

import matplotlib
from matplotlib import pylab as plt
import numpy as np

# Valid for the "before slot" variant.
_BRP_OVERHEAD = 4


def ParseJson(filename: str) -> dict:
  with open(filename, 'r') as f:
    data = json.load(f)
    return data


def PlotSuperPageData(data: dict, output_filename: str):
  num_superpages = len(data)
  num_partition_pages = len(data[0]['partition_pages'])
  data_np = np.zeros((num_superpages, num_partition_pages, 3))

  address_to_superpage = {superpage['address']: superpage for superpage in data}
  addresses = sorted(address_to_superpage.keys())

  BLACK = (0., 0., 0.)
  GRAY = (.5, .5, .5)
  RED = (1., 0., 0.)
  GREEN = (0., .8, 0.)
  WHITE = (1., 1., 1.)
  cmap = matplotlib.cm.get_cmap('coolwarm')

  for superpage_index in range(len(addresses)):
    superpage = address_to_superpage[addresses[superpage_index]]
    for index, partition_page in enumerate(superpage['partition_pages']):
      value = None
      if partition_page['type'] == 'metadata':
        value = BLACK
      elif partition_page['type'] == 'guard':
        value = GRAY
      elif partition_page['all_zeros'] and not 'is_active' in partition_page:
        # Otherwise it may be the subsequent partition page of a decommitted
        # slot span.
        if partition_page['page_index_in_span'] == 0:
          value = WHITE

      if value is not None:
        data_np[superpage_index, index] = value
        continue

      assert partition_page['type'] == 'payload'
      if partition_page['page_index_in_span'] == 0:
        num_partition_pages_in_slot_span = math.ceil(
            partition_page['num_system_pages_per_slot_span'] / 4)
        if partition_page['is_decommitted'] or partition_page['is_empty']:
          value = GREEN
        else:
          fullness = (partition_page['num_allocated_slots'] /
                      partition_page['slots_per_span'])
          value = cmap(fullness)[:3]
        data_np[superpage_index, index:index +
                num_partition_pages_in_slot_span, :] = value

  plt.figure(figsize=(20, len(address_to_superpage) / 2))
  plt.imshow(data_np)
  plt.title('Super page map')
  plt.yticks(ticks=range(len(addresses)), labels=addresses)
  plt.xlabel('PartitionPage index')

  handles = [
      matplotlib.patches.Patch(facecolor=BLACK, edgecolor='k',
                               label='Metadata'),
      matplotlib.patches.Patch(facecolor=GRAY, edgecolor='k', label='Guard'),
      matplotlib.patches.Patch(facecolor=WHITE, edgecolor='k', label='Empty'),
      matplotlib.patches.Patch(facecolor=GREEN,
                               edgecolor='k',
                               label='Decommitted'),
      matplotlib.patches.Patch(facecolor=cmap(0.),
                               edgecolor='k',
                               label='Committed Empty'),
      matplotlib.patches.Patch(facecolor=cmap(1.),
                               edgecolor='k',
                               label='Committed Full'),
  ]
  plt.legend(handles=handles, loc='lower left', fontsize=12, framealpha=.7)

  plt.savefig(output_filename, bbox_inches='tight')


def PlotSuperPageCompressionData(data: dict, output_filename: str):
  num_superpages = len(data)
  num_pages = len(data[0]['page_sizes'])
  data_np = np.zeros((num_superpages, num_pages, 3))

  address_to_superpage = {superpage['address']: superpage for superpage in data}
  addresses = sorted(address_to_superpage.keys())

  WHITE = (1., 1., 1.)
  cmap = matplotlib.cm.get_cmap('coolwarm')

  total_compressed_size = 0
  total_uncompressed_size = 0
  for superpage_index in range(len(addresses)):
    superpage = address_to_superpage[addresses[superpage_index]]
    for index, page in enumerate(superpage['page_sizes']):
      total_uncompressed_size += page['uncompressed']
      total_compressed_size += page['compressed']
      if page['uncompressed'] == 0:
        value = WHITE
      else:
        value = cmap(page['compressed'] / page['uncompressed'])[:3]
      data_np[superpage_index, index] = value

  overall_compression_ratio = (
      100. * total_compressed_size /
      (total_uncompressed_size)) if total_uncompressed_size else 0
  plt.figure(figsize=(20, len(address_to_superpage)))
  plt.imshow(data_np, aspect=8)
  plt.title('Super page compression ratio map - Ratio = %.1f%% '
            '- Compressed Size = %.1fMiB' %
            (overall_compression_ratio, total_compressed_size / (1 << 20)))
  plt.yticks(ticks=range(len(addresses)), labels=addresses)
  plt.xlabel('Page index in SuperPage')

  handles = [
      matplotlib.patches.Patch(facecolor=WHITE,
                               edgecolor='k',
                               label='Empty / Decommitted'),
      matplotlib.patches.Patch(facecolor=cmap(0.),
                               edgecolor='k',
                               label='Compressible'),
      matplotlib.patches.Patch(facecolor=cmap(1.),
                               edgecolor='k',
                               label='Incompressible'),
  ]
  plt.legend(handles=handles, loc='lower left', fontsize=12, framealpha=.7)
  plt.savefig(output_filename, bbox_inches='tight')


def _PlotFragmentationCommon(bucket_to_allocated: dict, output_filename: str):
  slot_sizes = sorted(bucket_to_allocated.keys())
  allocated = []
  waste = []
  for slot_size in slot_sizes:
    slots = np.array(bucket_to_allocated[slot_size])
    allocated.append(np.sum(slots))
    waste.append(np.sum(slot_size - slots))

  plt.figure(figsize=(18, 8))
  indices = range(len(slot_sizes))
  plt.bar(indices, allocated, label='Requested Memory')
  b = plt.bar(indices,
              waste,
              bottom=allocated,
              label='Waste due to padding + bucketing')

  waste_percentage = [('%d%%' % (int(100. * w / (w + a)))) if a else ''
                      for (w, a) in zip(waste, allocated)]

  plt.bar_label(b, labels=waste_percentage)
  plt.xticks(indices, slot_sizes, rotation='vertical')
  plt.xlim(left=-.5, right=len(indices))
  plt.xlabel('Bucket size')
  plt.ylabel('Memory (bytes)')
  plt.legend()

  total_allocated = np.sum(allocated)
  total_waste = np.sum(waste)
  plt.title('Absolute and relative waste due to bucketing and padding'
            ' per bucket - Total Allocated = %dMiB, Waste = %d%%' %
            (total_allocated /
             (1 << 20), int(100 * total_waste /
                            (total_allocated + total_waste))))
  plt.savefig(output_filename, bbox_inches='tight')


def _AdjustSizes(data: dict, bucket_sizes: list, adjustment_size: int) -> dict:
  requested_sizes = []
  for slot_span in data:
    requested_sizes += slot_span['allocated_sizes']

  # Assumes that all buckets have *some* live allocations.
  bucket_sizes = sorted(bucket_sizes)
  bucket_to_allocated = {slot_size: list() for slot_size in bucket_sizes}

  # Map the requested sizes without paddingb to buckets.
  for requested_size in requested_sizes:
    adjusted_size = requested_size + adjustment_size
    bucket_index = bisect.bisect_left(bucket_sizes, adjusted_size)
    slot_size = bucket_sizes[bucket_index]
    assert bucket_sizes[bucket_index] >= adjusted_size
    bucket_to_allocated[slot_size].append(adjusted_size)

  return bucket_to_allocated


def PlotSimulatedFragmentationData(data: dict, bucket_sizes: list,
                                   output_filename: str):
  # No adjustment, want to check waste without any padding.
  bucket_to_allocated = _AdjustSizes(data, bucket_sizes, 0)
  _PlotFragmentationCommon(bucket_to_allocated, output_filename)


def PlotFragmentationData(data: dict, bucket_sizes: list, output_filename: str):
  # "Before allocation" takes only 4 bytes, but the instrumentation added
  # expands this to 8 bytes. To reconstruct what it would have been without it,
  # take the requested size and add 4 bytes to it.
  bucket_to_allocated = _AdjustSizes(data, bucket_sizes, _BRP_OVERHEAD)
  _PlotFragmentationCommon(bucket_to_allocated, output_filename)


def PlotDelta(data: dict, bucket_sizes: list, output_filename: str):
  bucket_to_allocated_brp = _AdjustSizes(data, bucket_sizes, _BRP_OVERHEAD)
  bucket_to_allocated_no_brp = _AdjustSizes(data, bucket_sizes, 0)
  slot_sizes = sorted(bucket_to_allocated_no_brp)

  allocated_per_bucket_brp = {
      slot_size: len(bucket_to_allocated_brp[slot_size]) * slot_size
      for slot_size in bucket_to_allocated_brp
  }
  allocated_per_bucket_no_brp = {
      slot_size: len(bucket_to_allocated_no_brp[slot_size]) * slot_size
      for slot_size in bucket_to_allocated_no_brp
  }
  delta = [
      allocated_per_bucket_brp[slot_size] -
      allocated_per_bucket_no_brp[slot_size] for slot_size in slot_sizes
  ]

  plt.figure(figsize=(18, 8))
  indices = range(len(slot_sizes))
  plt.bar(indices, delta, label='Delta')
  plt.xticks(indices, slot_sizes, rotation='vertical')
  plt.xlim(left=-.5, right=len(indices))
  plt.xlabel('Bucket size')
  plt.ylabel('Memory delta (bytes)')
  plt.title('Per-bucket size difference BRP/No-BRP - Total = %.1fMiB' %
            (sum(delta) / (1 << 20)))
  plt.savefig(output_filename, bbox_inches='tight')


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--json',
                      help='JSON dump from pa_tcache_inspect',
                      required=True)
  parser.add_argument('--output', help='Output filename prefix', required=True)

  args = parser.parse_args()
  data = ParseJson(args.json)

  PlotSuperPageData(data['superpages'], args.output + '_superpage.png')
  PlotSuperPageCompressionData(data['superpages'],
                               args.output + '_compression.png')

  # Allocated object data is not always available.
  if 'allocated_sizes' in data:
    bucket_sizes = [x['slot_size'] for x in data['buckets']]
    PlotFragmentationData(data['allocated_sizes'], bucket_sizes,
                          args.output + '_waste.png')
    PlotSimulatedFragmentationData(data['allocated_sizes'], bucket_sizes,
                                   args.output + '_waste_simulated.png')
    PlotDelta(data['allocated_sizes'], bucket_sizes, args.output + '_delta.png')


if __name__ == '__main__':
  main()
