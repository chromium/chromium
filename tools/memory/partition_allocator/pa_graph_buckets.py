# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
This script allows us to create a live graph of internal fragmentation in
Chrome, updated as chrome runs.

Usage:
  1. Compile chrome with the RECORD_ALLOC_INFO flag.
  2. Compile pa_bucket_inspect tool with the RECORD_ALLOC_INFO flag.
  3. Start Chrome.
  4. Find the PID of the process you wish to create graphs for.
  5. run pa_bucket_inspect <PID>
  6. run this script.
"""

from sys import argv
import matplotlib.animation as animation
import matplotlib.pyplot as plt


def main(argv: list[str]) -> None:
  DUMPNAME: final = "dump.dat"

  fig = plt.figure(figsize=(16, 8))
  ax1 = fig.add_subplot(1, 1, 1)
  ax2 = ax1.twinx()

  def animate(i):
    bucket_sizes = []
    x = []
    y1 = []
    y2 = []
    with open(DUMPNAME, 'r') as f:
      for line in f.readlines():
        index, bucket_size, num_allocs, total_size, fragmentation = line.strip(
        ).split(',')
        print(index, bucket_size, num_allocs, total_size, fragmentation)
        x.append(int(index))
        # format buckets sizes with commas, e.g. 50000 -> 50,000
        bucket_sizes.append('{:,}'.format(int(bucket_size)))
        y1.append(int(fragmentation))
        y2.append(int(total_size) * int(fragmentation) / 100)

    ax1.clear()
    ax2.clear()
    ax1.set_xticks(x, bucket_sizes, rotation='vertical')
    ax2.set_xticks(x, bucket_sizes, rotation='vertical')
    plt.xlim(left=-.5, right=len(bucket_sizes))
    plt.xlabel('Bucket Size')
    ax1.set_ylabel('Internal Fragmentation (%)', color='g')
    ax2.set_ylabel('Wasted (MiB)', color='plum')
    plt.title('Internal Fragmentation vs Bucket Size')
    ax1.bar(x, y1, alpha=0.5, color='g')
    ax2.bar(x, y2, alpha=0.5, color='plum')

  ani = animation.FuncAnimation(fig, animate, interval=1000)
  plt.show()


if __name__ == '__main__':
  main(argv)
