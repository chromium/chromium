#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""From a native code residency dump created by log_residency.cc, generate a
visual timeline, and serialize the parsed data to JSON.
"""

import argparse
import json
import logging
from matplotlib import collections as mc
from matplotlib import pylab as plt
import numpy as np


def CreateArgumentParser():
  """Creates and returns an argument parser."""
  parser = argparse.ArgumentParser(
      description='Reads and shows native library residency data.')
  parser.add_argument('--dump', type=str, required=True, help='Residency dump')
  parser.add_argument('--output', type=str, required=True,
                      help='Output filename in text format')
  parser.add_argument('--json', type=str, help='Output filename in JSON output')
  return parser


def ParseDump(filename):
  """Parses a residency dump, as generated from orderfile_instrumentation.cc.

  Args:
    filename: (str) dump filename.

  Returns:
    {"start": offset, "end": offset,
     "residency": {timestamp (int): data ([bool])}}
  """
  result = {}
  with open(filename, 'r') as f:
    (start, end) = f.readline().strip().split(' ')
    result = {'start': int(start), 'end': int(end), 'residency': {}}
    for line in f:
      line = line.strip()
      timestamp, data = line.split(' ')
      data_array = [x == '1' for x in data]
      result['residency'][int(timestamp)] = data_array
  return result


def WriteJsonOutput(data, filename):
  """Serializes the parsed data to JSON.

  Args:
    data: (dict) As returned by ParseDump()
    filename: (str) output filename.

  JSON format:
  {'offset': int, 'data': {
    relative_timestamp: [{'page_offset': int, 'resident': bool}]}}

  Where:
    - offset is the code start offset into its page
    - relative_timestamp is the offset in ns since the first measurement
    - page_offset is the page offset in bytes
  """
  result = {'offset': data['start'], 'data': {}}
  start_timestamp = min(data['residency'].keys())
  for timestamp in data['residency']:
    adjusted_timestamp = timestamp - start_timestamp
    result[adjusted_timestamp] = []
    residency = data['residency'][timestamp]
    for (index, resident) in enumerate(residency):
      result[adjusted_timestamp].append(
          {'offset': index * (1 << 12), 'resident': resident})
  with open(filename, 'w') as f:
    json.dump(result, f)


def PlotResidency(data, output_filename):
  """Creates a graph of residency.

  Args:
    data: (dict) As returned by ParseDump().
    output_filename: (str) Output filename.
  """
  residency = data['residency']
  max_percentage = max((100. * sum(d)) / len(d) for d in residency.values())
  logging.info('Max residency = %.2f%%', max_percentage)

  start = data['start']
  end = data['end']
  _, ax = plt.subplots(figsize=(20, 10))
  timestamps = sorted(residency.keys())
  x_max = len(list(residency.values())[0]) * 4096
  for t in timestamps:
    offset_ms = (t - timestamps[0]) / 1e6
    incore = [i * 4096 for (i, x) in enumerate(residency[t]) if x]
    outcore = [i * 4096 for (i, x) in enumerate(residency[t]) if not x]
    percentage = 100. * len(incore) / (len(incore) + len(outcore))
    plt.text(x_max, offset_ms, '%.1f%%' % percentage)
    for (d, color) in ((incore, (.2, .6, .05, 1)), (outcore, (1, 0, 0, 1))):
      segments = [[(x, offset_ms), (x + 4096, offset_ms)] for x in d]
      colors = np.array([color] * len(segments))
      lc = mc.LineCollection(segments, colors=colors, linewidths=8)
      ax.add_collection(lc)

  plt.axvline(start)
  plt.axvline(end)
  plt.title('Code residency vs time since startup.')
  plt.xlabel('Code page offset (bytes)')
  plt.ylabel('Time since startup (ms)')
  plt.ylim(0, ymax=(timestamps[-1] - timestamps[0]) / 1e6)
  plt.xlim(xmin=0, xmax=x_max)
  plt.savefig(output_filename, bbox_inches='tight', dpi=300)


def main():
  parser = CreateArgumentParser()
  args = parser.parse_args()
  logging.basicConfig(level=logging.INFO)
  logging.info('Parsing the data')
  data = ParseDump(args.dump)
  if args.json:
    WriteJsonOutput(data, args.json)
  logging.info('Plotting the results')
  PlotResidency(data, args.output)


if __name__ == '__main__':
  main()
