# Copyright 2022 The Chromium Authors. All rights reserved.
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
  ax = fig.add_subplot(1, 1, 1)

  def animate(i):
    bucket_sizes = []
    x = []
    y = []
    with open(DUMPNAME, 'r') as f:
      for line in f.readlines():
        index, bucket_size, num_allocs, total_size, fragmentation = line.strip(
        ).split(',')
        print(index, bucket_size, num_allocs, total_size, fragmentation)
        x.append(int(index))
        bucket_sizes.append(int(bucket_size))
        y.append(int(fragmentation))

    ax.clear()
    plt.xticks(x, bucket_sizes, rotation='vertical')
    plt.ylim((0, 100))
    plt.xlim(left=-.5, right=len(bucket_sizes))
    plt.xlabel('Bucket Size')
    plt.ylabel('Internal Fragmentation (%)')
    plt.title('Internal Fragmentation vs Bucket Size')
    ax.bar(x, y, color='g')

  ani = animation.FuncAnimation(fig, animate, interval=1000)
  plt.show()


if __name__ == '__main__':
  main(argv)
