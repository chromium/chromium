#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Plots the distribution of allocated and unallocated slots in SlotSpans.

To run this:
1. Build pa_tcache_inspect at a "close-enough" revision to the Chrome instance
   you're interested in.
2. Run pa_tcache_inspect --pid=PID_OF_RUNNING_CHROME_PROCESS --json=output.json
3. Run plot_bucket_stats.py --json=output.json --output=output.png
"""

import argparse
import json

import matplotlib
from matplotlib import pylab as plt
import numpy as np


def ParseJson(filename):
  with open(filename, 'r') as f:
    data = json.load(f)
  return data


def PlotData(data, output_filename):
  rows, cols = 6, 5
  fig, axs = plt.subplots(rows, cols, figsize=(30, 30))
  fig.suptitle('Active Slot Spans fill-in - %s' % output_filename, fontsize=16)
  for index, bucket in enumerate(data['buckets'][:rows * cols]):
    ax = axs[int(index / cols), index % cols]
    active_slot_spans = bucket['active_slot_spans']

    freelist_sizes = np.array([
        s['freelist_size'] if not s['is_decommitted'] else 0
        for s in active_slot_spans
    ])
    allocated_slots = np.array(
        [s['num_allocated_slots'] for s in active_slot_spans])
    unprovisioned_slots = np.array(
        [s['num_unprovisioned_slots'] for s in active_slot_spans])
    hatch = ['//' if s['freelist_is_sorted'] else '' for s in active_slot_spans]

    slot_size = bucket['slot_size']
    allocated_size = bucket['allocated_slots'] * slot_size
    free_size = bucket['freelist_size'] * slot_size
    ax.set_title('Slot span = %d\nAllocated size = %dkiB, Free size = %dkiB' %
                 (slot_size, allocated_size // 1024, free_size // 1024))
    indices = range(len(active_slot_spans))
    bottom = np.zeros(len(indices))
    b1 = ax.bar(indices, allocated_slots, bottom=bottom, color='green')
    bottom += allocated_slots
    b2 = ax.bar(indices,
                freelist_sizes,
                bottom=bottom,
                color=[
                    'darkgrey' if s['is_decommitted'] else 'red'
                    for s in active_slot_spans
                ],
                hatch=hatch)
    bottom += freelist_sizes
    ax.bar(indices, unprovisioned_slots, bottom=bottom, color='lightgrey')
    ax.set_xlim(left=-.5, right=len(indices) - .5)

  fig.tight_layout()
  fig.subplots_adjust(top=.95)
  handles = [
      matplotlib.patches.Patch(facecolor='green', label='Allocated'),
      matplotlib.patches.Patch(facecolor='red',
                               hatch='//',
                               label='Sorted freelist'),
      matplotlib.patches.Patch(facecolor='red', label='Free'),
      matplotlib.patches.Patch(facecolor='grey', label='Unprovisioned'),
      matplotlib.patches.Patch(facecolor='darkgrey', label='Decommitted')
  ]
  fig.legend(handles=handles, loc='lower right', fontsize=20)
  plt.savefig(output_filename)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--json',
                      help='JSON dump from pa_tcache_inspect',
                      required=True)
  parser.add_argument('--output_prefix',
                      help='Output file prefix',
                      required=True)

  args = parser.parse_args()
  data = ParseJson(args.json)
  PlotData(data, args.output_prefix)


if __name__ == '__main__':
  main()
