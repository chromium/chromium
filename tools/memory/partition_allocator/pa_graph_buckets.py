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
import matplotlib.patches as mpatches


def main(argv: list[str]) -> None:
  DUMPNAME: final = "dump.dat"
  KIB: final = 1024
  MIB: final = KIB * 1024

  fig, axes = plt.subplots(2, 1)
  (ax_a, ax_b) = axes
  axes = ax_a, ax_b, ax_c, ax_d

  green_patch = mpatches.Patch(color='g', label='Used')
  plum_patch = mpatches.Patch(color='plum', label='Wasted')

  def animate(i):
    bucket_sizes = []
    x = []
    ya1 = []
    ya2 = []
    yb1 = []
    yb2 = []
    with open(DUMPNAME, 'r') as f:
      for line in f.readlines():
        index, bucket_size, num_allocs_a, total_requested_size_a, num_allocs_b, total_requested_size_b = [
            int(tmp) for tmp in line.strip().split(',')
        ]

        def record_allocs_and_sizes(y1, y2, num_allocs, total_requested_size):
          y1.append(bucket_size * num_allocs / MIB)
          y2.append((bucket_size * num_allocs - total_requested_size) / MIB)

        print(index, bucket_size, num_allocs_a, total_requested_size_a,
              num_allocs_b, total_requested_size_b)
        x.append((index))

        # format buckets sizes with commas, e.g. 50000 -> 50,000
        bucket_sizes.append('{:,}'.format(bucket_size))

        record_allocs_and_sizes(ya1, ya2, num_allocs_a, total_requested_size_a)
        record_allocs_and_sizes(yb1, yb2, num_allocs_b, total_requested_size_b)

    total_size_a = sum(ya1)
    total_size_b = sum(yb1)

    def plot_buckets(ax, x, y1, y2):
      ax.clear()
      ax.set_xticks(x, bucket_sizes, rotation='vertical')
      ax.set_xlabel('Bucket Size (B)')
      ax.set_ylabel('Total Memory Usage (MiB)')
      ax.bar(x, y1, color='g', width=0.8)
      ax.bar(x, y2, bottom=y1, color='plum', width=0.8)
      ax.legend(handles=[green_patch, plum_patch])

    plot_buckets(ax_a, x, ya1, ya2)
    plot_buckets(ax_b, x, yb1, yb2)

    # We want both plots to use the same y-height, so they can be compared
    # easily just by looking at them.
    h = max(ax.get_ylim() for ax in axes)
    for ax in axes:
      plt.setp(ax, ylim=h)

    def show_title(ax, total_size):
      diff = total_size - total_size_a
      ax.set_title(
          'Alternate Distribution uses an extra {:+.2f} KiB due to internal fragmentation ({:+.2%})'
          .format(diff * KIB, diff / total_size_a),
          size='medium')

    plt.suptitle('Memory Usage v. Bucket Size', size='x-large', weight='bold')
    show_title(ax_b, total_size_b)

  ani = animation.FuncAnimation(fig, animate, interval=1000)
  plt.tight_layout()
  plt.show()


if __name__ == '__main__':
  main(argv)
