#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import random
import unittest

import string_extract


def _FindBruteForce(string_data, value):
  """Reference implementation of StringDataIndex.Find()."""
  first_match = (-1, -1)
  for section_idx, data in enumerate(string_data):
    offset = -len(value)
    while True:
      offset = data.find(value, offset + len(value))
      if offset == -1:
        break
      if offset == 0 or data[offset - 1] == 0:
        return section_idx, offset
      if first_match[0] == -1:
        first_match = (section_idx, offset)
  return first_match


class StringExtractTest(unittest.TestCase):
  def testFind_Basic(self):
    string_data = [b'foo\0bar\0', b'oo\0xbar\0baz']
    index = string_extract.StringDataIndex(string_data)
    # Exact match.
    self.assertEqual((0, 4), index.Find(b'bar\0'))
    # Prefers exact match in a later section over a suffix match.
    self.assertEqual((1, 0), index.Find(b'oo\0'))
    # Suffix match in first section that has one.
    self.assertEqual((0, 5), index.Find(b'ar\0'))
    self.assertEqual((0, 2), index.Find(b'o\0'))
    # Unterminated data is never matched by a terminated value.
    self.assertEqual((-1, -1), index.Find(b'baz\0'))
    # Not found.
    self.assertEqual((-1, -1), index.Find(b'qux\0'))
    # Unterminated value.
    self.assertEqual((1, 8), index.Find(b'baz'))
    # Value with embedded \0.
    self.assertEqual((0, 0), index.Find(b'foo\0bar'))

  def testFind_MatchesBruteForce(self):
    rng = random.Random(0)
    alphabet = b'ab\0\xff'

    def RandBytes(max_len):
      return bytes(rng.choice(alphabet) for _ in range(rng.randint(0, max_len)))

    for _ in range(200):
      string_data = [RandBytes(40) for _ in range(rng.randint(1, 3))]
      index = string_extract.StringDataIndex(string_data)
      for _ in range(50):
        value = RandBytes(6)
        if rng.random() < 0.5:
          value += b'\0'
        self.assertEqual(
          _FindBruteForce(string_data, value),
          index.Find(value),
          f'string_data={string_data!r} value={value!r}',
        )


if __name__ == '__main__':
  unittest.main()
