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

  for superpage_index in range(len(addresses)):
    superpage = address_to_superpage[addresses[superpage_index]]
    for index, partition_page in enumerate(superpage['partition_pages']):
      value = 0
      if partition_page['type'] == 'metadata':
        value = (0., 0., 0.)
      elif partition_page['type'] == 'guard':
        value = (.5, .5, .5)
      elif partition_page['all_zeros']:
        value = (1., 0., 0.)
      else:
        value = (0., 1., 0.)
      data_np[superpage_index, index, :] = value

  plt.figure(figsize=(20, 14))
  plt.imshow(data_np)
  plt.title('Super page map')
  plt.yticks(ticks=range(len(addresses)), labels=addresses)
  plt.xlabel('PartitionPage index')

  handles = [
      matplotlib.patches.Patch(facecolor=(0., 0., 0.), label='Metadata'),
      matplotlib.patches.Patch(facecolor=(.5, .5, .5), label='Guard'),
      matplotlib.patches.Patch(facecolor=(1., 0., 0.), label='Free'),
      matplotlib.patches.Patch(facecolor=(0., 1., 0.), label='Allocated'),
  ]
  plt.legend(handles=handles, loc='lower right', fontsize=16)

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
