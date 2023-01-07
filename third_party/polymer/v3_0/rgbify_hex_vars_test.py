#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import rgbify_hex_vars
import unittest


class RgbifyHexVarsTest(unittest.TestCase):
  def checkProduces(self, content, expected, **kwargs):
    actual = rgbify_hex_vars.Rgbify(content, **kwargs)
    self.assertEqual(actual, expected)

  def checkSame(self, content):
    self.checkProduces(content, content)

  def testPrefixFiltering(self):
    self.checkProduces('--google-blue-500: #010203;\n' +
                       '--paper-green-300: #445566;',
                       '--google-blue-500-rgb: 1, 2, 3;\n' +
                       '--google-blue-500: #010203;\n' +
                       '--paper-green-300: #445566;',
                       prefix='google')

  def testReplace(self):
    self.checkProduces('--var-name: #01020f;',
                       '--var-name-rgb: 1, 2, 15;  /* #01020f */\n' +
                       '--var-name: rgb(var(--var-name-rgb));',
                       replace=True)

  def testStuffToBeIgnored(self):
    self.checkSame('#bada55 { color: red; }')
    self.checkSame('--var-name: rgb(1, 2, 3);')
    self.checkSame('--var-name: rgba(1, 2, 3, .5);')

  def testValidHexVars(self):
    self.checkProduces('--color-var: #010203;',
                       '--color-var-rgb: 1, 2, 3;\n' +
                       '--color-var: #010203;')
    self.checkProduces('--hi: #102030;',
                       '--hi-rgb: 16, 32, 48;\n' +
                       '--hi: #102030;')


if __name__ == '__main__':
  unittest.main()
