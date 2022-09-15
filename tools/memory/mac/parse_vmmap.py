#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Parses the output of vmmap to query IOSurface memory usage.
"""

import argparse
import collections
import logging
import operator
import re
import subprocess
import sys

IOSurface = collections.namedtuple('IOSurface', [
    'start', 'end', 'virtual', 'resident', 'dirty', 'swapped', 'width',
    'height', 'size'
])


def _FormatSize(n: int):
  _KIB = 1024
  _MIB = 1024 * _KIB
  result = ''
  if n > _MIB:
    result = '%.1fMiB' % (n / _MIB)
  else:
    result = '%0.1fkiB' % (n / _KIB)
  return result.rjust(8)


def IOSurfaceToString(iosurface: IOSurface):
  return ('%x-%x\tVirtual Size: %s\tResident: %s\tDirty: %s\tSwapped: %s'
          '\tDimensions: %dx%d') % (
              iosurface.start, iosurface.end, _FormatSize(iosurface.virtual),
              _FormatSize(iosurface.resident), _FormatSize(iosurface.dirty),
              _FormatSize(iosurface.swapped), iosurface.width, iosurface.height)


def ExecuteVmmap(pid: int) -> str:
  """Runs vmmap PID and returns its output."""
  ret = subprocess.run(['vmmap', str(pid)], capture_output=True)
  ret.check_returncode()
  stdout = ret.stdout.decode('utf-8')
  return stdout


def _ParseSize(size):
  suffix = size[-1]
  if suffix == 'K':
    return int(1024 * float(size[:-1]))
  elif suffix == 'M':
    return int(1024 * 1024 * float(size[:-1]))
  else:
    return int(size)


def _PrettyPrint(size: int) -> str:
  _KIB = 1024
  _MIB = 1024 * _KIB
  _GIB = 1024 * _MIB
  if size > _GIB:
    return '%.1fGiB' % (size / _GIB)
  elif size > _MIB:
    return '%.1fMiB' % (size / _MIB)
  elif size > _KIB:
    return '%.1fkiB' % (size / _KIB)
  else:
    return str(size)


def ParseIOSurface(contents: str, quiet=False) -> list:
  """From the content of a vmmap file, returns a list of IOSurfaces."""
  io_surfaces = []
  for line in contents.split('\n'):
    _ADDRESS_RE = '[0-9a-f]{1,16}'
    # Has an optional suffix, in which case it's a float
    _SIZE_RE = '[0-9\.KM]*'
    _SIZE_DECIMAL = '[0-9]*'
    _RE = ('^IOSurface *'
           '(?P<start>%(address)s)-(?P<end>%(address)s) *'
           '\[ *(?P<virtual>%(size)s) *(?P<resident>%(size)s) '
           '*(?P<dirty>%(size)s) *(?P<swapped>%(size)s)\]'
           '.*SurfaceID: 0x[0-9a-f]* *'
           '(?P<width>%(size_decimal)s)x(?P<height>%(size_decimal)s)'
           ' *\([^\(]*\) *'
           '(?P<size>%(size)s)') % {
               'address': _ADDRESS_RE,
               'size': _SIZE_RE,
               'size_decimal': _SIZE_DECIMAL
           }
    regexp = re.compile(_RE)
    _SAMPLE_LINE = ('IOSurface                   2a7d38000-2aae10000    '
                    '[ 48.8M     0K     0K  48.8M] rw-/rw- SM=SHM PURGE=N'
                    '  SurfaceID: 0x5f  2440x5196 (BGRA) 48.8M')
    assert regexp.match(_SAMPLE_LINE)
    _SAMPLE_LINE2 = ('IOSurface                   10c2d0000-10c2d4000    '
                     '[   16K     0K     0K     0K] rw-/rw- SM=SHM PURGE=N'
                     '  SurfaceID: 0x22e  19x19 (BGRA) 2560')
    assert regexp.match(_SAMPLE_LINE2)

    matches = regexp.match(line)
    if matches:
      if not quiet:
        print(line)
      start = int(matches.group('start'), 16)
      end = int(matches.group('end'), 16)
      virtual = _ParseSize(matches.group('virtual'))
      resident = _ParseSize(matches.group('resident'))
      dirty = _ParseSize(matches.group('dirty'))
      swapped = _ParseSize(matches.group('swapped'))
      width = int(matches.group('width'))
      height = int(matches.group('height'))
      size = _ParseSize(matches.group('size'))

      io_surface = IOSurface(start, end, virtual, resident, dirty, swapped,
                             width, height, size)
      io_surfaces.append(io_surface)
  return io_surfaces


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--pid',
                      help='PID of the process to get the maps of',
                      type=int)
  parser.add_argument('--filename',
                      help='Path to existing output of vmmap',
                      type=str)
  args = parser.parse_args()

  contents = None
  if args.pid:
    contents = ExecuteVmmap(args.pid)
  else:
    assert args.filename
    with open(args.filename, 'r') as f:
      contents = f.read()

  io_surfaces = ParseIOSurface(contents)
  io_surfaces.sort(key=operator.attrgetter('virtual'))
  print('\nIOSurfaces sorted by virtual size:')
  for io_surface in io_surfaces:
    print('\t' + IOSurfaceToString(io_surface))

  io_surfaces.sort(key=operator.attrgetter('width'))
  print('\nIOSurfaces sorted by width:')
  for io_surface in io_surfaces:
    print('\t' + IOSurfaceToString(io_surface))

  io_surfaces.sort(key=lambda x: x.dirty + x.swapped)
  print('\nIOSurfaces sorted by dirty/swapped:')
  for io_surface in io_surfaces:
    print('\t' + IOSurfaceToString(io_surface))

  lost_to_paging = sum(io_surface.virtual - io_surface.size
                       for io_surface in io_surfaces)
  dirty = sum(io_surface.dirty for io_surface in io_surfaces)
  swapped = sum(io_surface.swapped for io_surface in io_surfaces)
  print('\nMemory lost due to page rounding = %s' %
        _PrettyPrint(lost_to_paging))
  print('Dirty Memory = %s' % _PrettyPrint(dirty))
  print('Swapped Memory = %s' % _PrettyPrint(swapped))


if __name__ == '__main__':
  main()
