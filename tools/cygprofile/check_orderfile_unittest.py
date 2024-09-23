#!/usr/bin/env vpython3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import check_orderfile
import symbol_extractor


class TestCheckOrderFile(unittest.TestCase):
  _SYMBOL_INFOS = [
      symbol_extractor.SymbolInfo('first', 0x1, 0, ''),
      symbol_extractor.SymbolInfo('second', 0x2, 0, ''),
      symbol_extractor.SymbolInfo('secondSameOffsetA', 0x2, 0, ''),
      symbol_extractor.SymbolInfo('secondSameOffsetB', 0x2, 0, ''),
      symbol_extractor.SymbolInfo('notProfiled', 0x4, 0, ''),
      symbol_extractor.SymbolInfo('third', 0x3, 0, ''),
      symbol_extractor.SymbolInfo('foo.llvm.123', 0x5, 0, ''),
      symbol_extractor.SymbolInfo('atTwoAddresses', 0x6, 0, ''),
      symbol_extractor.SymbolInfo('between', 0x6, 0, ''),
      symbol_extractor.SymbolInfo('atTwoAddresses', 0x8, 0, ''),
  ]

  def testVerifySymbolOrder(self):
    self.assertTrue(
        check_orderfile._VerifySymbolOrder(
            ['nonexistent', 'first', 'eighth', 'third'], self._SYMBOL_INFOS, 0))
    self.assertFalse(
        check_orderfile._VerifySymbolOrder(
            ['second', 'first', 'eighth', 'third'], self._SYMBOL_INFOS, 0))
    self.assertTrue(
        check_orderfile._VerifySymbolOrder(
            ['second', 'first', 'eighth', 'third'], self._SYMBOL_INFOS, 1))
    self.assertFalse(
        check_orderfile._VerifySymbolOrder(
            ['foo.llvm.123', 'first', 'eighth', 'third'], self._SYMBOL_INFOS,
            0))
    self.assertTrue(
        check_orderfile._VerifySymbolOrder(
            ['first', 'eighth', 'third', 'foo.llvm.123'], self._SYMBOL_INFOS,
            0))

  def testVerifySameOffset(self):
    self.assertTrue(
        check_orderfile._VerifySymbolOrder(
            ['first', 'second', 'third', 'secondSameOffsetA'],
            self._SYMBOL_INFOS, 0))
    self.assertTrue(
        check_orderfile._VerifySymbolOrder([
            'first', 'second', 'third', 'secondSameOffsetA', 'foo.llvm.123',
            'secondSameOffsetB'
        ], self._SYMBOL_INFOS, 0))
    self.assertTrue(
        check_orderfile._VerifySymbolOrder(
            ['first', 'secondSameOffsetB', 'third', 'second'],
            self._SYMBOL_INFOS, 0))
    self.assertFalse(
        check_orderfile._VerifySymbolOrder(
            ['first', 'third', 'second', 'secondSameOffsetA'],
            self._SYMBOL_INFOS, 0))

  def testSymbolAtMultipleAddressesIsIgnored(self):
    self.assertTrue(
        check_orderfile._VerifySymbolOrder(['between', 'atTwoAddresses'],
                                           self._SYMBOL_INFOS, 0))
    self.assertTrue(
        check_orderfile._VerifySymbolOrder(['atTwoAddresses', 'between'],
                                           self._SYMBOL_INFOS, 0))

if __name__ == '__main__':
  unittest.main()
