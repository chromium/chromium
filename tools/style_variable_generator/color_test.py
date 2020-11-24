# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from color import Color
import unittest


class ColorTest(unittest.TestCase):
    def testHexColors(self):
        c = Color('#0102ff')
        self.assertEqual(c.r, 1)
        self.assertEqual(c.g, 2)
        self.assertEqual(c.b, 255)
        self.assertEqual(c.a, 1)

    def testRGBColors(self):
        c = Color('rgb(100, 200, 123)')
        self.assertEqual(c.r, 100)
        self.assertEqual(c.g, 200)
        self.assertEqual(c.b, 123)
        self.assertEqual(c.a, 1)

        c = Color('rgb($some_color_rgb)')
        self.assertEqual(c.rgb_var, 'some_color_rgb')
        self.assertEqual(c.a, 1)

    def testRGBAColors(self):
        c = Color('rgba(100, 200, 123, 0.5)')
        self.assertEqual(c.r, 100)
        self.assertEqual(c.g, 200)
        self.assertEqual(c.b, 123)
        self.assertEqual(c.a, 0.5)

        c = Color('rgba($some_color_400_rgb, 0.1)')
        self.assertEqual(c.rgb_var, 'some_color_400_rgb')
        self.assertEqual(c.a, 0.1)

    def testReferenceColor(self):
        c = Color('$some_color')
        self.assertEqual(c.var, 'some_color')

    def testWhiteBlackColor(self):
        c = Color('$white')
        self.assertEqual((c.r, c.g, c.b, c.a), (255, 255, 255, 1))

        c = Color('rgba($white_rgb, 0.5)')
        self.assertEqual((c.r, c.g, c.b, c.a), (255, 255, 255, 0.5))

        c = Color('$black')
        self.assertEqual((c.r, c.g, c.b, c.a), (0, 0, 0, 1))

        c = Color('rgba($black_rgb, 0.5)')
        self.assertEqual((c.r, c.g, c.b, c.a), (0, 0, 0, 0.5))

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
            Color('$some_color_rgb')

        with self.assertRaises(ValueError):
            # Variable reference with accidental space.
            print(Color('$some_color_rgb '))

        with self.assertRaises(ValueError):
            # Variable reference with accidental space.
            Color('rgba($non_ rgb_var, 0.4)')


if __name__ == '__main__':
    unittest.main()
