#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import trybot_commit_size_checker


class Tests(unittest.TestCase):
  def testIterForTestingSymbolsFromMapping(self):
    def test_case(snippet, expected):
      actual = list(
          trybot_commit_size_checker.IterForTestingSymbolsFromMapping(snippet))
      self.assertEqual(expected, actual)

    # Test ignored comments and non-ForTest symbols
    test_case(
        """
# pkg.CommentForTesting > o:
pkg.NormalClass -> p:
    android.os.IBinder mRemote -> o
    1:3:android.os.IBinder asBinder():168:168 -> asBinder
    3:6:Bundle foo(java.lang.StringForTest):250:250 -> b
        # {"id":"residualsignature","signature":"()LBundleForTest;"}
""", [])

    # Test when class has ForTest in it.
    test_case(
        """
pkg.ClzForTest -> Ja1:
    java.lang.String DESCRIPTOR -> b
    7:13:void <clinit>():290:290 -> <clinit>
    7:13:FooForTest pkg2.ForTesting.someMethod():290:290 -> b
    7:13:void pkg2.Clz.setForTests():290:290 -> b
""", [
            'pkg.ClzForTest',
            'pkg.ClzForTest#DESCRIPTOR',
            'pkg.ClzForTest#<clinit>',
            'pkg2.ForTesting#someMethod',
            'pkg2.Clz#setForTests',
        ])

    # Test when class does not have ForTest in it.
    test_case(
        """
pkg.Clz -> Ja1:
    java.lang.String fieldForTest -> b
    java.lang.String FIELD_FOR_TEST -> b
    7:13:void <clinit>():290:290 -> <clinit>
    7:13:FooForTest pkg2.ForTesting.someMethod():290:290 -> b
    7:13:void pkg2.Clz.setForTests():290:290 -> b
""", [
            'pkg.Clz#fieldForTest',
            'pkg.Clz#FIELD_FOR_TEST',
            'pkg2.ForTesting#someMethod',
            'pkg2.Clz#setForTests',
        ])


if __name__ == '__main__':
  unittest.main()
