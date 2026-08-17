#!/usr/bin/env python
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from io import StringIO
import logging
import os
import sys
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

from procfs import ProcMaps


class ProcMapsTest(unittest.TestCase):
  _TEST_PROCMAPS = '\n'.join(
    [
      '00000000-00001000 r--p 00000000 fc:00 0',
      '0080b000-0080c000 r-xp 0020b000 fc:00 2231329'
      '                            /usr/bin/some',
      '0080c000-0080f000 ---p 0020c000 fc:00 2231329'
      '                            /usr/bin/some',
      '0100a000-0100c000 r-xp 0120a000 fc:00 22381'
      '                            /usr/bin/chrome',
      '0100c000-0100f000 ---p 0120c000 fc:00 22381'
      '                            /usr/bin/chrome',
      '0237d000-02a9b000 rw-p 00000000 00:00 0'
      '                                  [heap]',
      '7fb920e6d000-7fb920e85000 r-xp 00000000 fc:00 263482'
      '                     /lib/x86_64-linux-gnu/libpthread-2.15.so',
      '7fb920e85000-7fb921084000 ---p 00018000 fc:00 263482'
      '                     /lib/x86_64-linux-gnu/libpthread-2.15.so',
      '7fb9225f4000-7fb922654000 rw-s 00000000 00:04 19660808'
      '                   /SYSV00000000 (deleted)',
      'ffffffffff600000-ffffffffff601000 r-xp 00000000 00:00 0'
      '                  [vsyscall]',
    ]
  )

  _EXPECTED = [
    (0x0, 0x1000, 'r', '-', '-', 'p', 0x0, 'fc', '00', 0, ''),
    (
      0x80B000,
      0x80C000,
      'r',
      '-',
      'x',
      'p',
      0x20B000,
      'fc',
      '00',
      2231329,
      '/usr/bin/some',
    ),
    (
      0x80C000,
      0x80F000,
      '-',
      '-',
      '-',
      'p',
      0x20C000,
      'fc',
      '00',
      2231329,
      '/usr/bin/some',
    ),
    (
      0x100A000,
      0x100C000,
      'r',
      '-',
      'x',
      'p',
      0x120A000,
      'fc',
      '00',
      22381,
      '/usr/bin/chrome',
    ),
    (
      0x100C000,
      0x100F000,
      '-',
      '-',
      '-',
      'p',
      0x120C000,
      'fc',
      '00',
      22381,
      '/usr/bin/chrome',
    ),
    (0x237D000, 0x2A9B000, 'r', 'w', '-', 'p', 0x0, '00', '00', 0, '[heap]'),
    (
      0x7FB920E6D000,
      0x7FB920E85000,
      'r',
      '-',
      'x',
      'p',
      0x0,
      'fc',
      '00',
      263482,
      '/lib/x86_64-linux-gnu/libpthread-2.15.so',
    ),
    (
      0x7FB920E85000,
      0x7FB921084000,
      '-',
      '-',
      '-',
      'p',
      0x18000,
      'fc',
      '00',
      263482,
      '/lib/x86_64-linux-gnu/libpthread-2.15.so',
    ),
    (
      0x7FB9225F4000,
      0x7FB922654000,
      'r',
      'w',
      '-',
      's',
      0x0,
      '00',
      '04',
      19660808,
      '/SYSV00000000 (deleted)',
    ),
    (
      0xFFFFFFFFFF600000,
      0xFFFFFFFFFF601000,
      'r',
      '-',
      'x',
      'p',
      0x0,
      '00',
      '00',
      0,
      '[vsyscall]',
    ),
  ]

  @staticmethod
  def _expected_as_dict(index):
    return {
      'begin': ProcMapsTest._EXPECTED[index][0],
      'end': ProcMapsTest._EXPECTED[index][1],
      'readable': ProcMapsTest._EXPECTED[index][2],
      'writable': ProcMapsTest._EXPECTED[index][3],
      'executable': ProcMapsTest._EXPECTED[index][4],
      'private': ProcMapsTest._EXPECTED[index][5],
      'offset': ProcMapsTest._EXPECTED[index][6],
      'major': ProcMapsTest._EXPECTED[index][7],
      'minor': ProcMapsTest._EXPECTED[index][8],
      'inode': ProcMapsTest._EXPECTED[index][9],
      'name': ProcMapsTest._EXPECTED[index][10],
    }

  def test_load(self):
    maps = ProcMaps.load_file(StringIO(self._TEST_PROCMAPS))
    for index, entry in enumerate(maps):
      self.assertEqual(entry.as_dict(), self._expected_as_dict(index))

  def test_constants(self):
    maps = ProcMaps.load_file(StringIO(self._TEST_PROCMAPS))
    selected = [0, 2, 4, 7]
    for index, entry in enumerate(maps.iter(ProcMaps.constants)):
      self.assertEqual(entry.as_dict(), self._expected_as_dict(selected[index]))

  def test_executable(self):
    maps = ProcMaps.load_file(StringIO(self._TEST_PROCMAPS))
    selected = [1, 3, 6, 9]
    for index, entry in enumerate(maps.iter(ProcMaps.executable)):
      self.assertEqual(entry.as_dict(), self._expected_as_dict(selected[index]))

  def test_executable_and_constants(self):
    maps = ProcMaps.load_file(StringIO(self._TEST_PROCMAPS))
    selected = [0, 1, 2, 3, 4, 6, 7, 9]
    for index, entry in enumerate(maps.iter(ProcMaps.executable_and_constants)):
      self.assertEqual(entry.as_dict(), self._expected_as_dict(selected[index]))


if __name__ == '__main__':
  logging.basicConfig(
    level=logging.DEBUG if '-v' in sys.argv else logging.ERROR,
    format='%(levelname)5s %(filename)15s(%(lineno)3d): %(message)s',
  )
  unittest.main()
