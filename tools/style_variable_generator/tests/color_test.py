#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
from pathlib import Path

if len(Path(__file__).parents) > 2:
    sys.path += [str(Path(__file__).parents[2])]

from style_variable_generator.color import Color, split_args
import unittest


class ColorTest(unittest.TestCase):
    def testHexColors(self):
        c = Color('#0102ff')
        self.assertEqual(c.r, 1)
        self.assertEqual(c.g, 2)
        self.assertEqual(c.b, 255)
        self.assertEqual(c.opacity.a, 1)

    def testRGBColors(self):
        c = Color('rgb(100, 200, 123)')
        self.assertEqual(c.r, 100)
        self.assertEqual(c.g, 200)
        self.assertEqual(c.b, 123)
        self.assertEqual(c.opacity.a, 1)

        c = Color('rgb($some_color.rgb)')
        self.assertEqual(c.rgb_var, 'some_color.rgb')
        self.assertEqual(c.opacity.a, 1)

    def testRGBAColors(self):
        c = Color('rgba(100, 200, 123, 0.5)')
        self.assertEqual(c.r, 100)
        self.assertEqual(c.g, 200)
        self.assertEqual(c.b, 123)
        self.assertEqual(c.opacity.a, 0.5)

        c = Color('rgba($some_color_400.rgb, 0.1)')
        self.assertEqual(c.rgb_var, 'some_color_400.rgb')
        self.assertEqual(c.opacity.a, 0.1)

    def testLegacyRGBRef(self):
        c = Color('rgba($some_color_400_rgb, 0.1)')
        self.assertEqual(c.rgb_var, 'some_color_400.rgb')
        self.assertEqual(c.opacity.a, 0.1)

    def testBlendColors(self):
        # White over Grey 900.
        c = Color('blend($white, #00ff00)')
        self.assertEqual(len(c.blended_colors), 2)
        c0 = c.blended_colors[0]
        self.assertEqual(c0.r, 255)
        self.assertEqual(c0.g, 255)
        self.assertEqual(c0.b, 255)
        self.assertEqual(c0.opacity.a, 1)
        c1 = c.blended_colors[1]
        self.assertEqual(c1.r, 0)
        self.assertEqual(c1.g, 255)
        self.assertEqual(c1.b, 0)
        self.assertEqual(c1.opacity.a, 1)

        # Some color 6% over Grey 900 60%.
        c = Color('blend(rgba($some_color.rgb, 0.06), rgba(32, 33, 36, 0.6))')
        self.assertEqual(len(c.blended_colors), 2)
        c0 = c.blended_colors[0]
        self.assertEqual(c0.rgb_var, 'some_color.rgb')
        self.assertEqual(c0.opacity.a, 0.06)
        c1 = c.blended_colors[1]
        self.assertEqual(c1.r, 32)
        self.assertEqual(c1.g, 33)
        self.assertEqual(c1.b, 36)
        self.assertEqual(c1.opacity.a, 0.6)

    def testReferenceColor(self):
        c = Color('$some_color')
        self.assertEqual(c.var, 'some_color')

    def testWhiteBlackColor(self):
        c = Color('$white')
        self.assertEqual((c.r, c.g, c.b, c.opacity.a), (255, 255, 255, 1))

        c = Color('rgba($white.rgb, 0.5)')
        self.assertEqual((c.r, c.g, c.b, c.opacity.a), (255, 255, 255, 0.5))

        c = Color('$black')
        self.assertEqual((c.r, c.g, c.b, c.opacity.a), (0, 0, 0, 1))

        c = Color('rgba($black.rgb, 0.5)')
        self.assertEqual((c.r, c.g, c.b, c.opacity.a), (0, 0, 0, 0.5))

    def testMalformedColors(self):
        with self.assertRaises(ValueError):
            # #RRGGBBAA not supported.
            Color('#11223311')

        with self.assertRaises(ValueError):
            # #RGB not supported.
            Color('#fff')

        with self.assertRaises(ValueError):
            Color('rgb($non_rgb_var)')

        with self.assertRaises(ValueError):
            Color('rgba($non_rgb_var, 0.4)')

        with self.assertRaises(ValueError):
            # Invalid alpha.
            Color('rgba(1, 2, 4, 2.5)')

        with self.assertRaises(ValueError):
            # Invalid alpha.
            Color('rgba($non_rgb_var, -1)')

        with self.assertRaises(ValueError):
            # Invalid rgb values.
            Color('rgb(-1, 5, 5)')

        with self.assertRaises(ValueError):
            # Invalid rgb values.
            Color('rgb(0, 256, 5)')

        with self.assertRaises(ValueError):
            # Color reference points to rgb reference.
            Color('$some_color.rgb')

        with self.assertRaises(ValueError):
            # Variable reference with accidental space.
            print(Color('$some_color.rgb '))

        with self.assertRaises(ValueError):
            # Variable reference with accidental space.
            Color('rgba($non_ rgb_var, 0.4)')

    def testSplitArgs(self):
        args = list(split_args('a'))
        self.assertEqual(args, ['a'])

        args = list(split_args('a, b, c'))
        self.assertEqual(args, ['a', 'b', 'c'])

        args = list(split_args('foo(), bar(x), baz(x, y)'))
        self.assertEqual(args, ['foo()', 'bar(x)', 'baz(x, y)'])

        args = list(split_args('foo(bar(a, b, c))'))
        self.assertEqual(args, ['foo(bar(a, b, c))'])

        with self.assertRaises(ValueError):
            # Too many ")".
            list(split_args('foo(bar(a, b)) )'))

        with self.assertRaises(ValueError):
            # Too many "(".
            list(split_args('foo(bar(a, b)) ('))


if __name__ == '__main__':
    unittest.main()
