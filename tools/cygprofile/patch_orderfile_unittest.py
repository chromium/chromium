#!/usr/bin/env vpython3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import patch_orderfile
import symbol_extractor


class TestPatchOrderFile(unittest.TestCase):
  def testRemoveSuffixes(self):
    no_clone = 'this.does.not.contain.clone'
    self.assertEquals(no_clone, patch_orderfile.RemoveSuffixes(no_clone))
    with_clone = 'this.does.contain.clone.'
    self.assertEquals(
        'this.does.contain', patch_orderfile.RemoveSuffixes(with_clone))
    with_part = 'this.is.a.part.42'
    self.assertEquals(
        'this.is.a', patch_orderfile.RemoveSuffixes(with_part))

  def testUniqueGenerator(self):
    @patch_orderfile._UniqueGenerator
    def TestIterator():
      yield 1
      yield 2
      yield 1
      yield 3

    self.assertEqual(list(TestIterator()), [1,2,3])

  def testMaxOutlinedIndex(self):
    self.assertEquals(7, patch_orderfile._GetMaxOutlinedIndex(
        {'OUTLINED_FUNCTION_{}'.format(idx): None
         for idx in [1, 2, 3, 7]}))
    self.assertRaises(AssertionError, patch_orderfile._GetMaxOutlinedIndex,
                      {'OUTLINED_FUNCTION_{}'.format(idx): None
                       for idx in [1, 200, 3, 11]})
    self.assertEquals(None, patch_orderfile._GetMaxOutlinedIndex(
        {'a': None, 'b': None}))

  def testPatchedSymbols(self):
    # From input symbols a b c d, symbols a and d match themselves, symbol
    # b matches b and x, and symbol c is missing.
    self.assertEquals(list('abxd'),
                      list(patch_orderfile._PatchedSymbols(
                          {'a': 'a', 'b': 'bx', 'd': 'd'},
                          'abcd', None)))

  def testPatchedSymbolsWithOutlining(self):
    # As above, but add outlined functions at the end. The aliased outlined
    # function should be ignored.
    self.assertEquals(
        list('abd') + ['OUTLINED_FUNCTION_{}'.format(i) for i in range(5)],
        list(
            patch_orderfile._PatchedSymbols(
                {
                    'a': 'a',
                    'b': ['b', 'OUTLINED_FUNCTION_4'],
                    'd': 'd'
                }, ['a', 'b', 'OUTLINED_FUNCTION_2', 'c', 'd'], 2)))


if __name__ == '__main__':
  unittest.main()
