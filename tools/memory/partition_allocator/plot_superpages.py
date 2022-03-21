#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Maps the superpage occupancy.

To run this:

1. Build pa_dump_heap at a close-enough revision to the Chrome instance you're
   interested in.
2. Run pa_dump_heap --pid=PID_OF_RUNNING_CHROME_PROCESS --json=output.json
3. Run plot_superpages.py --json=output.json --output=output.png
"""

import argparse
import math
import json

import matplotlib
from matplotlib import pylab as plt
import numpy as np


def ParseJson(filename):
  with open(filename, 'r') as f:
    data = json.load(f)
    return data


def PlotData(data, output_filename):
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


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--json',
                      help='JSON dump from pa_tcache_inspect',
                      required=True)
  parser.add_argument('--output', help='Output file', required=True)

  args = parser.parse_args()
  data = ParseJson(args.json)
  PlotData(data, args.output)


if __name__ == '__main__':
  main()
