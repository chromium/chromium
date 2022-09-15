#!/usr/bin/env vpython3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import check_orderfile
import symbol_extractor


class TestCheckOrderFile(unittest.TestCase):
  _SYMBOL_INFOS = [symbol_extractor.SymbolInfo('first', 0x1, 0, ''),
                   symbol_extractor.SymbolInfo('second', 0x2, 0, ''),
                   symbol_extractor.SymbolInfo('notProfiled', 0x4, 0, ''),
                   symbol_extractor.SymbolInfo('third', 0x3, 0, ''),]

  def testVerifySymbolOrder(self):
    self.assertTrue(check_orderfile._VerifySymbolOrder(
        ['.second', 'first', 'eighth', 'third'],
        self._SYMBOL_INFOS, 0))
    self.assertFalse(check_orderfile._VerifySymbolOrder(
        ['second', 'first', 'eighth', 'third'],
        self._SYMBOL_INFOS, 0))
    self.assertTrue(check_orderfile._VerifySymbolOrder(
        ['second', 'first', 'eighth', 'third'],
        self._SYMBOL_INFOS, 1))


if __name__ == '__main__':
  unittest.main()
