#!/usr/bin/env python
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Reduces result of 'readelf -wL' to just a list of starting addresses.

It lists up all addresses where the corresponding source files change.  The
list is sorted in ascending order.  See tests/reduce_debugline_test.py for
examples.

This script assumes that the result of 'readelf -wL' ends with an empty line.

Note: the option '-wL' has the same meaning with '--debug-dump=decodedline'.
"""

from __future__ import print_function

import re
import sys


_FILENAME_PATTERN = re.compile('(CU: |)(.+)\:')


def reduce_decoded_debugline(input_file):
  filename = ''
  starting_dict = {}
  started = False

  for line in input_file:
    line = line.strip()
    unpacked = line.split(None, 2)

    if len(unpacked) == 3 and unpacked[2].startswith('0x'):
      if not started and filename:
        started = True
        starting_dict[int(unpacked[2], 16)] = filename
    else:
      started = False
      if line.endswith(':'):
        matched = _FILENAME_PATTERN.match(line)
        if matched:
          filename = matched.group(2)

  starting_list = []
  prev_filename = ''
  for address in sorted(starting_dict):
    curr_filename = starting_dict[address]
    if prev_filename != curr_filename:
      starting_list.append((address, starting_dict[address]))
    prev_filename = curr_filename
  return starting_list


def main():
  if len(sys.argv) != 1:
    print('Unsupported arguments', file=sys.stderr)
    return 1

  starting_list = reduce_decoded_debugline(sys.stdin)
  bits64 = starting_list[-1][0] > 0xffffffff
  for address, filename in starting_list:
    if bits64:
      print('%016x %s' % (address, filename))
    else:
      print('%08x %s' % (address, filename))
  return 0


if __name__ == '__main__':
  sys.exit(main())
