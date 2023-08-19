#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from make_gtest_filter import PascalCaseSplit, CompressWithWildcards, GetFiltersForTests


class Foo(unittest.TestCase):
  def testSplit(self):
    self.assertEqual(list(PascalCaseSplit('testTerm')), ['test', 'Term'])
    self.assertEqual(list(PascalCaseSplit('TestTerm')), ['Test', 'Term'])
    self.assertEqual(list(PascalCaseSplit('TestTerm80')),
                     ['Test', 'Term', '80'])
    self.assertEqual(list(PascalCaseSplit('TestTerm80Foo')),
                     ['Test', 'Term', '80', 'Foo'])
    self.assertEqual(list(PascalCaseSplit('TestTerm80/Foo')),
                     ['Test', 'Term', '80', '/', 'Foo'])

  def testDepth(self):
    melons = ['A.DoomMelonFooBar', 'A.DoomMelonFooBaz']
    self.assertEqual(list(CompressWithWildcards(melons, 1, 0)), ['A.Doom*'])
    self.assertEqual(list(CompressWithWildcards(melons, 2, 0)),
                     ['A.DoomMelon*'])
    self.assertEqual(list(CompressWithWildcards(melons, 3, 0)),
                     ['A.DoomMelonFoo*'])
    self.assertEqual(list(CompressWithWildcards(melons, 4, 0)),
                     ['A.DoomMelonFooBar', 'A.DoomMelonFooBaz'])

  def testDontWildcardAcrossSuites(self):
    self.assertEqual(list(CompressWithWildcards(['A.X', 'B.X'], 1, 0)),
                     ['A.X', 'B.X'])

  def testCaseNumBoundaryBeforeWildcard(self):
    fruit = ['A.DoomMelonFooBar', 'A.DoomMelonFooBaz', 'A.DoomBanana']
    self.assertEqual(list(CompressWithWildcards(fruit, 2, 0)),
                     ['A.DoomBanana', 'A.DoomMelon*'])
    self.assertEqual(list(CompressWithWildcards(fruit, 2, 1)),
                     ['A.DoomBanana', 'A.DoomMelon*'])
    self.assertEqual(list(CompressWithWildcards(fruit, 2, 2)),
                     ['A.DoomBanana', 'A.DoomMelonFooBar', 'A.DoomMelonFooBaz'])

  def testGetFiltersForTests(self):
    tests = ['TestSuite.TestName']
    self.assertEqual(list(GetFiltersForTests(tests, class_only=True)), [
        'TestSuite.*', '*/TestSuite.*/*', '*/TestSuite/*.*', 'TestSuite.*/*',
        'TestSuite/*.*'
    ])
    self.assertEqual(list(GetFiltersForTests(tests, class_only=False)), [
        'TestSuite.TestName', '*/TestSuite.TestName/*', 'TestSuite.TestName/*',
        'TestSuite/*.TestName'
    ])


if __name__ == '__main__':
  unittest.main()
