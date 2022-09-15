#!/usr/bin/env python
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import compact_blocklist as cb
import unittest


class CompactBlocklistTest(unittest.TestCase):
  def TestValidUUID(self):
    self.assertTrue( cb.ValidUUID('00000000-0000-0000-0000-000000000000'))
    self.assertTrue( cb.ValidUUID('01234567-89ab-cdef-0123-456789abcdef'))
    self.assertTrue( cb.ValidUUID('00001812-0000-1000-8000-00805f9b34fb'))
    self.assertFalse(cb.ValidUUID('g1234567-89ab-cdef-0123-456789abcdef'))
    self.assertFalse(cb.ValidUUID('01234567-89ab-cdef-0123-456789abcdef0'))
    self.assertFalse(cb.ValidUUID('0123456789abcdef0123456789abcdef'))
    self.assertFalse(cb.ValidUUID('01234567089ab0cdef001230456789abcdef'))

  def TestShortenUUID(self):
    self.assertEqual(cb.ShortenUUID(
      '00000000-0000-0000-0000-000000000000'),
      '00000000-0000-0000-0000-000000000000')
    self.assertEqual(cb.ShortenUUID(
      '01234567-89ab-cdef-0123-456789abcdef'),
      '01234567-89ab-cdef-0123-456789abcdef')
    self.assertEqual(cb.ShortenUUID(
      '00001812-0000-1000-8000-00805f9b34fb'),
      '1812')
    self.assertEqual(cb.ShortenUUID(
      'g1234567-89ab-cdef-0123-456789abcdef'),
      'g1234567-89ab-cdef-0123-456789abcdef')
    self.assertEqual(cb.ShortenUUID(
      '01234567-89ab-cdef-0123-456789abcdef0'),
      '01234567-89ab-cdef-0123-456789abcdef0')
    self.assertEqual(cb.ShortenUUID(
      '0123456789abcdef0123456789abcdef'),
      '0123456789abcdef0123456789abcdef')
    self.assertEqual(cb.ShortenUUID(
      '01234567089ab0cdef001230456789abcdef'),
      '01234567089ab0cdef001230456789abcdef')

  def TestProcess(self):
    blocklist = collections.OrderedDict()
    try:
      cb.Process('# comment', blocklist)
      cb.Process('', blocklist)
    except Exception:
      self.fail('Failed test for comment or blank line.')

    self.assertRaises(cb.BadLineException, cb.Process,
      '00001812-0000-1000-8000-00805f9b34fb exclude-write exclude', blocklist)
    self.assertRaises(cb.InvalidExclusionException, cb.Process,
      '00001812-0000-1000-8000-00805f9b34fb exclude-write', blocklist)
    self.assertRaises(cb.InvalidExclusionException, cb.Process,
      '00001812-0000-1000-8000-00805f9b34fb exclude', blocklist)

    try:
      cb.Process('00001812-0000-1000-8000-00805f9b34fa exclude-writes',
        blocklist)
      cb.Process('00001812-0000-1000-8000-00805f9b34fb exclude-reads',
        blocklist)
      cb.Process('00001812-0000-1000-8000-00805f9b34fc', blocklist)
    except Exception:
      self.fail('Failed test for valid lines.')

    self.assertRaises(cb.DuplicateUUIDException, cb.Process,
      '00001812-0000-1000-8000-00805f9b34fa exclude-writes', blocklist)


if __name__ == '__main__':
  unittest.main()
