# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for disable.py"""

import unittest
import disable


class DisableTest(unittest.TestCase):
  def test_parse_bug(self):
    self.assertEqual(disable.parse_bug('1234'), (1234, 'chromium'))
    self.assertEqual(disable.parse_bug('crbug/9871'), (9871, 'chromium'))
    self.assertEqual(disable.parse_bug('https://crbug/9871'),
                     (9871, 'chromium'))
    self.assertEqual(disable.parse_bug('crbug/v8/111111'), (111111, 'v8'))
    self.assertEqual(disable.parse_bug('https://crbug/v8/111111'),
                     (111111, 'v8'))
    self.assertEqual(disable.parse_bug('crbug.com/8'), (8, 'chromium'))
    self.assertEqual(disable.parse_bug('https://crbug.com/8'), (8, 'chromium'))
    self.assertEqual(disable.parse_bug('crbug.com/monorail/19782757'),
                     (19782757, 'monorail'))
    self.assertEqual(disable.parse_bug('https://crbug.com/monorail/19782757'),
                     (19782757, 'monorail'))
    self.assertEqual(
        disable.parse_bug('bugs.chromium.org/p/foo/issues/detail?id=6060842'),
        (6060842, 'foo'))
    self.assertEqual(
        disable.parse_bug(
            'http://bugs.chromium.org/p/foo/issues/detail?id=6060842'),
        (6060842, 'foo'))
    self.assertEqual(
        disable.parse_bug(
            'https://bugs.chromium.org/p/foo/issues/detail?id=6060842'),
        (6060842, 'foo'))
    self.assertEqual(
        disable.parse_bug('https://bugs.chromium.org/p/foo/issues/detail' +
                          '?id=191972&q=owner%3Ame%20link&can=2'),
        (191972, 'foo'))


if __name__ == '__main__':
  unittest.main()
