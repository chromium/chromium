#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Visualizes the amount of IOSurface memory used over time.
"""

import argparse
import time

import matplotlib
from matplotlib import pylab as plt
import matplotlib.animation as animation
import numpy as np

import parse_vmmap


def _PlotData(pid: int):
  times = []
  virtual = []
  dirty = []
  swapped = []

  fig = plt.figure(figsize=(16, 8))
  ax = fig.add_subplot(1, 1, 1)
  first_time = time.time()

  def _Animate(i):
    contents = parse_vmmap.ExecuteVmmap(pid)
    io_surfaces = parse_vmmap.ParseIOSurface(contents, quiet=True)
    total_virtual_size = sum(io_surface.size for io_surface in io_surfaces)
    total_dirty_size = sum(io_surface.dirty for io_surface in io_surfaces)
    total_swapped_size = sum(io_surface.swapped for io_surface in io_surfaces)

    print('SIZE = %d\tDIRTY = %d\tSWAPPED = %d' %
          (total_virtual_size, total_dirty_size, total_swapped_size))
    now = time.time()

    times.append(now - first_time)
    _MIB = 1 << 20
    virtual.append(total_virtual_size / _MIB)
    dirty.append(total_dirty_size / _MIB)
    swapped.append(total_swapped_size / _MIB)
    bucket_sizes = []

    ax.clear()
    bottom = np.zeros(len(times))

    top = np.array(swapped)
    ax.plot(times, top, label='Swapped')
    plt.fill_between(times, top, bottom)
    bottom += swapped

    top += dirty
    ax.plot(times, top, label='Dirty')
    plt.fill_between(times, top, bottom)
    bottom += dirty

    top = virtual
    ax.plot(times, top, label='Virtual')
    plt.fill_between(times, top, bottom)

    plt.ylim(bottom=0)
    plt.xlim(left=times[0], right=times[-1])

    plt.xlabel('Time (s)')
    plt.ylabel('Memory (MiB)')
    plt.title('IOSurface memory usage vs time - PID = %d' % pid)

    plt.legend()

  ani = animation.FuncAnimation(fig, _Animate, interval=1000)
  plt.show()


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--pid',
                      help='PID of the GPU process',
                      type=int,
                      required=True)
  args = parser.parse_args()

  _PlotData(args.pid)


if __name__ == '__main__':
  main()
