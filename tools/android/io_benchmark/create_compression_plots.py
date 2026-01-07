#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Plots compression benchmark data from a log file."""
"""Example output from the compression benchmark, collected on a Pixel 9 Pro
XL:

snappy,compression,4096,642.656,6.37355,2.18638
snappy,decompression,4096,2464.02,1.66232,0
zlib,compression,4096,61.8968,66.1747,3.11847
zlib,decompression,4096,462.005,8.8657,0
brotli,compression,4096,79.3817,51.5988,2.98319
brotli,decompression,4096,325.685,12.5766,0
zstd,compression,4096,251.261,16.3018,2.88243
zstd,decompression,4096,624.912,6.55452,0
snappy,compression,8192,643.191,12.7365,2.39172
snappy,decompression,8192,2509.8,3.264,0
zlib,compression,8192,73.2304,111.866,3.50879
zlib,decompression,8192,576.15,14.2185,0
brotli,compression,8192,103.024,79.5157,3.33781
brotli,decompression,8192,378.119,21.6651,0
zstd,compression,8192,282.574,28.9907,3.20382
zstd,decompression,8192,693.977,11.8044,0
snappy,compression,16384,649.352,25.2313,2.56941
snappy,decompression,16384,2602.04,6.29661,0
zlib,compression,16384,78.6679,208.268,3.85982
zlib,decompression,16384,696.758,23.5146,0
brotli,compression,16384,99.3363,164.935,3.65392
brotli,decompression,16384,437.289,37.4672,0
zstd,compression,16384,299.34,54.7337,3.48034
zstd,decompression,16384,697.936,23.4749,0
snappy,compression,32768,648.755,50.509,2.72575
snappy,decompression,32768,2665.05,12.2955,0
zlib,compression,32768,76.8604,426.331,4.17712
zlib,decompression,32768,805.511,40.6797,0
brotli,compression,32768,115.891,282.749,3.92706
brotli,decompression,32768,482.838,67.8654,0
zstd,compression,32768,311.749,105.11,3.65447
zstd,decompression,32768,648.521,50.5272,0
snappy,compression,65536,640.877,102.26,2.85657
snappy,decompression,65536,2518.7,26.0198,0
zlib,compression,65536,68.8598,951.73,4.42353
zlib,decompression,65536,679.309,96.4745,0
brotli,compression,65536,104.125,629.396,4.15997
brotli,decompression,65536,428.744,152.856,0
zstd,compression,65536,287.076,228.288,3.84273
zstd,decompression,65536,635.617,103.106,0
snappy,compression,131072,563.095,232.771,2.85675
snappy,decompression,131072,2069.03,63.3496,0
zlib,compression,131072,48.207,2718.94,4.56282
zlib,decompression,131072,599.625,218.59,0
brotli,compression,131072,98.0048,1337.4,4.35204
brotli,decompression,131072,366.146,357.978,0
zstd,compression,131072,231.972,565.035,3.98334
zstd,decompression,131072,522.312,250.946,0
snappy,compression,262144,417.379,628.071,2.85743
snappy,decompression,262144,1766.36,148.409,0
zlib,compression,262144,43.4555,6032.47,4.64006
zlib,decompression,262144,615.425,425.956,0
brotli,compression,262144,101.789,2575.37,4.5071
brotli,decompression,262144,425.305,616.367,0
zstd,compression,262144,259.054,1011.93,4.15231
zstd,decompression,262144,670.871,390.752,0
snappy,compression,524288,512.186,1023.63,2.85868
snappy,decompression,524288,2036.45,257.452,0
zlib,compression,524288,46.3367,11314.7,4.68096
zlib,decompression,524288,677.152,774.255,0
brotli,compression,524288,114.686,4571.51,4.62552
brotli,decompression,524288,451.508,1161.19,0
zstd,compression,524288,263.804,1987.42,4.17699
zstd,decompression,524288,709.376,739.083,0
snappy,compression,1048576,432.182,2426.24,2.86094
snappy,decompression,1048576,1171.7,894.917,0
zlib,compression,1048576,50.7317,20669.1,4.70398
zlib,decompression,1048576,629.12,1666.73,0
brotli,compression,1048576,123.968,8458.42,4.70563
brotli,decompression,1048576,404.745,2590.71,0
zstd,compression,1048576,300.14,3493.62,4.22404
zstd,decompression,1048576,671.651,1561.19,0
snappy,compression,2097152,477.989,4387.44,2.86533
snappy,decompression,2097152,1490.91,1406.62,0
zlib,compression,2097152,54.5512,38443.8,4.72124
zlib,decompression,2097152,711.711,2946.64,0
brotli,compression,2097152,142.115,14756.7,4.7608
brotli,decompression,2097152,402.27,5213.29,0
zstd,compression,2097152,322.759,6497.58,4.25403
zstd,decompression,2097152,720.164,2912.05,0
"""

import argparse
import os
import re
import sys

import matplotlib.pyplot as plt
import pandas as pd


def ParseData(filepath: str) -> pd.DataFrame:
  """Parses the benchmark data from the given file.

    Args:
      filepath: File to parse, from the output of the script.
    """
  data = []
  line_regex = re.compile(r'(\w+),'  # method (e.g., snappy)
                          r'(compression|decompression),'  # operation
                          r'(\d+),'  # chunk_size
                          r'([\d.]+),'  # throughput
                          r'([\d.]+),'  # latency
                          r'([\d.]+)'  # compression_ratio
                          r'$')

  with open(filepath, 'r') as f:
    for line in f:
      match = line_regex.search(line)
      if match:
        data.append(list(match.groups()))

  df = pd.DataFrame(data,
                    columns=[
                        'method', 'operation', 'chunk_size', 'throughput',
                        'latency', 'compression_ratio'
                    ])

  for col in ['chunk_size', 'throughput', 'latency', 'compression_ratio']:
    df[col] = pd.to_numeric(df[col])
  return df


def Plot(df: pd.DataFrame, output_dir: str = '.') -> None:
  """Generates and saves plots from the benchmark data.

    Args:
      df: As returned by ParseData().
      output_dir: base directory to output the plots
    """
  methods = sorted(df['method'].unique())

  # Ensure output directory exists
  os.makedirs(output_dir, exist_ok=True)

  compression_df = df[df['operation'] == 'compression']
  decompression_df = df[df['operation'] == 'decompression']

  def CreatePlot(data,
                 y_col,
                 title,
                 ylabel,
                 is_log_y=False,
                 output_filename=""):
    plt.figure(figsize=(12, 7))
    for method in methods:
      subset = data[data['method'] == method]
      if not subset.empty:
        plt.plot(subset['chunk_size'],
                 subset[y_col],
                 marker='o',
                 linestyle='-',
                 label=method)

    plt.title(title)
    plt.xlabel('Chunk Size (bytes)')
    plt.ylabel(ylabel)
    plt.xscale('log', base=2)
    if is_log_y:
      plt.yscale('log')
    plt.grid(True, which="both", ls="--")
    plt.legend()
    plt.savefig(os.path.join(output_dir, output_filename))
    plt.close()

  CreatePlot(compression_df,
             'throughput',
             'Compression Throughput vs. Chunk Size',
             'Throughput (MB/s)',
             output_filename='compression_throughput.png')

  CreatePlot(decompression_df,
             'throughput',
             'Decompression Throughput vs. Chunk Size',
             'Throughput (MB/s)',
             output_filename='decompression_throughput.png')

  CreatePlot(compression_df,
             'latency',
             'Compression Latency vs. Chunk Size',
             'Latency (microseconds)',
             is_log_y=True,
             output_filename='compression_latency.png')

  CreatePlot(decompression_df,
             'latency',
             'Decompression Latency vs. Chunk Size',
             'Latency (microseconds)',
             is_log_y=True,
             output_filename='decompression_latency.png')

  CreatePlot(compression_df,
             'compression_ratio',
             'Compression Ratio vs. Chunk Size',
             'Compression Ratio',
             output_filename='compression_ratio.png')


def main() -> int:
  parser = argparse.ArgumentParser(
      description='Parse and plot compression benchmark data.')
  parser.add_argument('input_file',
                      help='Path to the input file with benchmark data.')
  parser.add_argument('output_dir',
                      nargs='?',
                      default='.',
                      help='Directory to save the plots.')
  args = parser.parse_args()

  if not os.path.exists(args.input_file):
    print(f"Error: Input file '{args.input_file}' not found.", file=sys.stderr)
    return 1

  df = ParseData(args.input_file)
  Plot(df, args.output_dir)
  print("Generated benchmark plots in "
        f"'{os.path.abspath(args.output_dir)}'")


if __name__ == '__main__':
  main()
