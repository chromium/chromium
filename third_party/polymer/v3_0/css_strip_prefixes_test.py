#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import css_strip_prefixes
import unittest


class CssStripPrefixesTest(unittest.TestCase):
  def testShouldRemoveLineTrue(self):
    # Test case where the prefixed property is before the the colon.
    self.assertTrue(css_strip_prefixes.ShouldRemoveLine('-ms-flex: bar;'))
    self.assertTrue(css_strip_prefixes.ShouldRemoveLine('-ms-flex:bar;'))
    self.assertTrue(css_strip_prefixes.ShouldRemoveLine('  -ms-flex: bar; '))
    self.assertTrue(css_strip_prefixes.ShouldRemoveLine('  -ms-flex: bar ;'))

    # Test case where the prefixed property is after the the colon.
    self.assertTrue(
        css_strip_prefixes.ShouldRemoveLine(' display: -ms-inline-flexbox;'))

    # Test lines with comments also get removed.
    self.assertTrue(css_strip_prefixes.ShouldRemoveLine(
        ' display: -ms-inline-flexbox; /* */'))
    self.assertTrue(css_strip_prefixes.ShouldRemoveLine(
        ' -ms-flex: bar; /* foo */ '))

  def testShouldRemoveLineFalse(self):
    # Test cases where the line is not considered a CSS line.
    self.assertFalse(css_strip_prefixes.ShouldRemoveLine(''))
    self.assertFalse(css_strip_prefixes.ShouldRemoveLine(' -ms-flex'))
    self.assertFalse(css_strip_prefixes.ShouldRemoveLine('/* -ms-flex */'))
    self.assertFalse(css_strip_prefixes.ShouldRemoveLine(': -ms-flex; '))
    self.assertFalse(css_strip_prefixes.ShouldRemoveLine(' : -ms-flex; '))
    self.assertFalse(css_strip_prefixes.ShouldRemoveLine('-ms-flex {'))

    # Test cases where prefixed CSS rules should be unaffected.
    css_to_preserve = [
      '-webkit-appearance',
      '-webkit-box-reflect',
      '-webkit-font-smoothing',
      '-webkit-overflow-scrolling',
      '-webkit-tap-highlight',
    ]
    for p in css_to_preserve:
      self.assertFalse(css_strip_prefixes.ShouldRemoveLine(p + ': bar;'))


if __name__ == '__main__':
  unittest.main()
