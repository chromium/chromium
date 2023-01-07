#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Extract physical addresses (PFNs) from /proc/PID/pagemap files.

This can be useful, for example, to compare whether two addresses from
potentially two different processes map to the same physical page. The virtual
addresses required for this script can be obtained from log messages, while the
pagemap files can be copied or fetched from an Android device using `adb pull`.
"""

import argparse
import logging
import os
import struct
import sys

_BYTES_PER_PAGEMAP_VALUE = 8
_BYTES_PER_OS_PAGE = 4096
_VIRTUAL_TO_PAGEMAP_OFFSET = _BYTES_PER_OS_PAGE // _BYTES_PER_PAGEMAP_VALUE
_MASK_PRESENT = 1 << 63
_MASK_PFN = (1 << 55) - 1


def PrintPfn(fd, vaddr):
  os.lseek(fd, vaddr // _VIRTUAL_TO_PAGEMAP_OFFSET, os.SEEK_SET)
  buf = os.read(fd, _BYTES_PER_PAGEMAP_VALUE)
  if len(buf) < _BYTES_PER_PAGEMAP_VALUE:
    logging.error('Could not retrieve the pagemap entry')
    return False
  pagemap_values = struct.unpack(
      '=%dQ' % (len(buf) // _BYTES_PER_PAGEMAP_VALUE), buf)
  for pagemap_value in pagemap_values:
    if pagemap_value & _MASK_PRESENT:
      print(hex(pagemap_value & _MASK_PFN))
    else:
      logging.error('Page not present: %s', hex(vaddr))
      return False
  return True


def main():
  logging.getLogger().setLevel(logging.INFO)
  parser = argparse.ArgumentParser()
  parser.add_argument('--pagemap-file',
                      required=True,
                      help='Path to a saved /proc/pid/pagemap file.')
  parser.add_argument('--vaddr',
                      required=True,
                      help='Virtual address (in hex) to inspect.')
  args = parser.parse_args()
  fd = os.open(args.pagemap_file, os.O_RDONLY)
  if not PrintPfn(fd, int(args.vaddr, 16)):
    return 1
  return 0


if __name__ == '__main__':
  sys.exit(main())
