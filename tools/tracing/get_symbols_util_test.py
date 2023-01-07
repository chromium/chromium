#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import get_symbols_util


class GetSymbolsUtilTestCase(unittest.TestCase):
  def testMangleModuleId(self):
    self.assertEqual(get_symbols_util.MangleModuleIfNeeded(''),
                     '00000000000000000000000000000000')
    self.assertEqual(get_symbols_util.MangleModuleIfNeeded('ABC'),
                     'ABC00000000000000000000000000000')
    self.assertEqual(get_symbols_util.MangleModuleIfNeeded('abc'),
                     'ABC00000000000000000000000000000')

    self.assertEqual(
        get_symbols_util.MangleModuleIfNeeded(
            '3DDFC247136ABA2328FB4E0EF678654C0'),
        '3DDFC247136ABA2328FB4E0EF678654C0')
    self.assertEqual(
        get_symbols_util.MangleModuleIfNeeded(
            '7F0715C286F8B16C10E4AD349CDA3B9B56C7A773'),
        'C215077FF8866CB110E4AD349CDA3B9B0')


if __name__ == '__main__':
  unittest.main()
