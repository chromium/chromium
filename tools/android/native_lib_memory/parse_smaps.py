#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Parses /proc/[pid]/smaps on a device and shows the total amount of swap used.
"""

import argparse
import collections
import logging
import os
import re
import sys

_SRC_PATH = os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir, os.pardir)
sys.path.append(os.path.join(_SRC_PATH, 'third_party', 'catapult', 'devil'))
from devil.android import device_utils


class Mapping:
  """A single entry (mapping) in /proc/[pid]/smaps."""

  def __init__(self, start, end, permissions, offset, pathname):
    """Initializes an instance.

    Args:
      start: (str) Start address of the mapping.
      end: (str) End address of the mapping.
      permissions: (str) Permission string, e.g. r-wp.
      offset: (str) Offset into the file or 0 if this is not a file mapping.
      pathname: (str) Path name, or pseudo-path, e.g. [stack]
    """
    self.start = int(start, 16)
    self.end = int(end, 16)
    self.permissions = permissions
    self.offset = int(offset, 16)
    self.pathname = pathname.strip()
    self.fields = collections.OrderedDict()

  def AddField(self, line):
    """Adds a field to an entry.

    Args:
      line: (str) As it appears in /proc/[pid]/smaps.
    """
    assert ':' in line
    split_index = line.index(':')
    k, v = line[:split_index].strip(), line[split_index + 1:].strip()
    assert k not in self.fields
    if v.endswith('kB'):
      v = int(v[:-2])
    self.fields[k] = v

  def ToString(self):
    """Returns a string representation of a mapping.

    The returned string is similar (but not identical) to the /proc/[pid]/smaps
    entry it was generated from.
    """
    lines = []
    lines.append('%x-%x %s %x %s' % (
        self.start, self.end, self.permissions, self.offset, self.pathname))
    for name in self.fields:
      format_str = None
      if isinstance(self.fields[name], int):
        format_str = '%s: %d kB'
      else:
        format_str = '%s: %s'
      lines.append(format_str % (name, self.fields[name]))
    return '\n'.join(lines)


def _ParseProcSmapsLines(lines):
  SMAPS_ENTRY_START_RE = (
      # start-end
      '^([0-9a-f]{1,16})-([0-9a-f]{1,16}) '
      # Permissions
      '([r\-][w\-][x\-][ps]) '
      # Offset
      '([0-9a-f]{1,16}) '
      # Device
      '([0-9a-f]{2,3}:[0-9a-f]{2,3}) '
      # Inode
      '([0-9]*) '
      # Pathname
      '(.*)')
  assert re.search(SMAPS_ENTRY_START_RE,
                   '35b1800000-35b1820000 r-xp 00000000 08:02 135522  '
                   '/usr/lib64/ld-2.15.so')
  entry_re = re.compile(SMAPS_ENTRY_START_RE)

  mappings = []
  for line in lines:
    match = entry_re.search(line)
    if match:
      (start, end, perms, offset, _, _, pathname) = match.groups()
      mappings.append(Mapping(start, end, perms, offset, pathname))
    else:
      mappings[-1].AddField(line)
  return mappings


def ParseProcSmaps(device, pid, store_file=False):
  """Parses /proc/[pid]/smaps on a device, and returns a list of Mapping.

  Args:
    device: (device_utils.DeviceUtils) device to parse the file from.
    pid: (int) PID of the process.
    store_file: (bool) Whether to also write the file to disk.

  Returns:
    [Mapping] all the mappings in /proc/[pid]/smaps.
  """
  command = ['cat', '/proc/%d/smaps' % pid]
  lines = device.RunShellCommand(command, check_return=True)
  if store_file:
    with open('smaps-%d' % pid, 'w') as f:
      f.write('\n'.join(lines))
  return _ParseProcSmapsLines(lines)


def _GetPageTableFootprint(device, pid):
  """Returns the page table footprint for a process in kiB."""
  command = ['cat', '/proc/%d/status' % pid]
  lines = device.RunShellCommand(command, check_return=True)
  for line in lines:
    if line.startswith('VmPTE:'):
      value = int(line[len('VmPTE: '):line.index('kB')])
      return value
  # Should not be reached.
  return None


def _SummarizeMapping(mapping, metric):
  return '%s %s %s: %d kB (Total Size: %d kB)' % (
      hex(mapping.start),
      mapping.pathname, mapping.permissions, metric,
      (mapping.end - mapping.start) / 1024)


def _PrintMappingsMetric(mappings, field_name):
  """Shows a summary of mappings for a given metric.

  For the given field, compute its aggregate value over all mappings, and
  prints the mappings sorted by decreasing metric value.

  Args:
    mappings: ([Mapping]) all process mappings.
    field_name: (str) Mapping field to process.
  """
  total_kb = sum(m.fields[field_name] for m in mappings)
  print('Total Size (kB) = %d' % total_kb)
  sorted_by_metric = sorted(mappings,
                            key=lambda m: m.fields[field_name], reverse=True)
  for mapping in sorted_by_metric:
    metric = mapping.fields[field_name]
    if not metric:
      break
    print(_SummarizeMapping(mapping, metric))


def _PrintSwapStats(mappings):
  print('SWAP:')
  _PrintMappingsMetric(mappings, 'Swap')


def _PrintAnonymousMappingsStats(mappings):
  print('Anonymous mappings sorted by Shared_Dirty memory:')
  anonymous_mappings = [
      mapping for mapping in mappings if mapping.pathname.startswith('[')
  ]
  _PrintMappingsMetric(anonymous_mappings, 'Shared_Dirty')

  print('\n\nAnonymous mappings sorted by RSS:')
  _PrintMappingsMetric(anonymous_mappings, 'Rss')


def _FootprintForAnonymousMapping(mapping):
  assert mapping.pathname.startswith('[anon:')
  if (mapping.pathname == '[anon:libc_malloc]'
      and mapping.fields['Shared_Dirty'] != 0):
    # libc_malloc mappings can come from the zygote. In this case, the shared
    # dirty memory is likely dirty in the zygote, don't count it.
    return mapping.fields['Rss']
  return mapping.fields['Private_Dirty']


def _PrintEstimatedFootprintStats(mappings, page_table_kb):
  print('Private Dirty:')
  _PrintMappingsMetric(mappings, 'Private_Dirty')
  print('\n\nShared Dirty:')
  _PrintMappingsMetric(mappings, 'Shared_Dirty')
  print('\n\nPrivate Clean:')
  _PrintMappingsMetric(mappings, 'Private_Clean')
  print('\n\nShared Clean:')
  _PrintMappingsMetric(mappings, 'Shared_Clean')
  print('\n\nSwap PSS:')
  _PrintMappingsMetric(mappings, 'SwapPss')
  print('\n\nPage table = %d kiB' % page_table_kb)


def _ComputeEstimatedFootprint(mappings, page_table_kb):
  """Returns the estimated footprint in kiB.

  Args:
    mappings: ([Mapping]) all process mappings.
    page_table_kb: (int) Sizeof the page tables in kiB.
  """
  footprint = page_table_kb
  for mapping in mappings:
    # Chrome shared memory.
    #
    # Even though it is shared memory, it exists because the process exists, so
    # account for its entirety.
    if mapping.pathname.startswith('/dev/ashmem/shared_memory'):
      footprint += mapping.fields['Rss']
    elif mapping.pathname.startswith('[anon'):
      footprint += _FootprintForAnonymousMapping(mapping)
    # Mappings without a name are most likely Chrome's native memory allocators:
    # v8, PartitionAlloc, Oilpan.
    # All of it should be charged to our process.
    elif mapping.pathname.strip() == '':
      footprint += mapping.fields['Rss']
    # Often inherited from the zygote, only count the private dirty part,
    # especially as the swap part likely comes from the zygote.
    elif mapping.pathname.startswith('['):
      footprint += mapping.fields['Private_Dirty']
    # File mappings. Can be a real file, and/or Dalvik/ART.
    else:
      footprint += mapping.fields['Private_Dirty']
  return footprint


def _ShowAllocatorFootprint(mappings, allocator):
  """Shows the total footprint from a specific allocator.

  Args:
    mappings: ([Mapping]) all process mappings.
    allocator: (str) Allocator name.
  """
  total_footprint = 0
  pathname = '[anon:%s]' % allocator
  for mapping in mappings:
    if mapping.pathname == pathname:
      total_footprint += _FootprintForAnonymousMapping(mapping)
  print('\tFootprint from %s: %d kB' % (allocator, total_footprint))


def _CreateArgumentParser():
  parser = argparse.ArgumentParser()
  parser.add_argument('--pid', help='PID.', type=int)
  parser.add_argument('--file', help='Pre-stored file', type=str)
  parser.add_argument('--estimate-footprint',
                      help='Show the estimated memory foootprint',
                      action='store_true')
  parser.add_argument('--store-smaps', help='Store the smaps file locally',
                      action='store_true')
  parser.add_argument('--show-allocator-footprint',
                      help='Show the footprint from a given allocator',
                      choices=['v8', 'libc_malloc', 'partition_alloc'],
                      nargs='+')
  parser.add_argument(
      '--device', help='Device to use', type=str, default='default')
  return parser


def main():
  parser = _CreateArgumentParser()
  args = parser.parse_args()

  mappings = None

  if args.file:
    lines = []
    with open(args.file, 'r') as f:
      lines = f.readlines()
    mappings = _ParseProcSmapsLines(lines)
  else:
    devices = device_utils.DeviceUtils.HealthyDevices(device_arg=args.device)
    if not devices:
      logging.error('No connected devices')
      return

    device = devices[0]
    if not device.HasRoot():
      device.EnableRoot()
    mappings = ParseProcSmaps(device, args.pid, args.store_smaps)

  # Enable logging after device handling as devil is noisy at INFO level.
  logging.basicConfig(level=logging.INFO)

  if args.estimate_footprint:
    page_table_kb = _GetPageTableFootprint(device, args.pid)
    _PrintEstimatedFootprintStats(mappings, page_table_kb)
    footprint = _ComputeEstimatedFootprint(mappings, page_table_kb)
    print('\n\nEstimated Footprint = %d kiB' % footprint)
  else:
    _PrintSwapStats(mappings)
    print('\n\n\n')
    _PrintAnonymousMappingsStats(mappings)

  if args.show_allocator_footprint:
    print('\n\nMemory Allocators footprint:')
    for allocator in args.show_allocator_footprint:
      _ShowAllocatorFootprint(mappings, allocator)


if __name__ == '__main__':
  main()
