#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import apkanalyzer


class ApkAnalyzerTest(unittest.TestCase):
  def assertEqualLists(self, list1, list2):
    self.assertEqual(set(list1), set(list2))

  def testUndoHierarchicalSizing_Empty(self):
    data = []
    nodes = apkanalyzer.UndoHierarchicalSizing(data)
    self.assertEqual(0, len(nodes))

  def testUndoHierarchicalSizing_TotalSingleRootNode(self):
    data = [
      ('P', '<TOTAL>', 5),
    ]
    nodes = apkanalyzer.UndoHierarchicalSizing(data)
    # No changes expected since there are no child nodes.
    self.assertEqualLists(data, nodes)

  def testUndoHierarchicalSizing_TotalSizeMinusChildNode(self):
    data = [
      ('P', '<TOTAL>', 10),
      ('C', 'child1', 7),
    ]
    nodes = apkanalyzer.UndoHierarchicalSizing(data)
    self.assertEqualLists([
      ('P', '<TOTAL>', 3),
      ('C', 'child1', 7),
    ], nodes)

  def testUndoHierarchicalSizing_SiblingAnonymousClass(self):
    data = [
      ('C', 'class1', 10),
      ('C', 'class1$inner', 8),
    ]
    nodes = apkanalyzer.UndoHierarchicalSizing(data)
    # No change in size expected since these should be siblings.
    self.assertEqualLists(data, nodes)

  def testUndoHierarchicalSizing_MethodsShouldBeChildNodes(self):
    data = [
      ('C', 'class1', 10),
      ('M', 'class1 method', 8),
    ]
    nodes = apkanalyzer.UndoHierarchicalSizing(data)
    self.assertEqualLists([
      ('C', 'class1', 2),
      ('M', 'class1 method', 8),
    ], nodes)

  def testUndoHierarchicalSizing_ClassIsChildNodeOfPackage(self):
    data = [
      ('P', 'package1', 10),
      ('C', 'package1.class1', 10),
    ]
    nodes = apkanalyzer.UndoHierarchicalSizing(data)
    self.assertEqualLists([
      ('C', 'package1.class1', 10),
    ], nodes)

  def testUndoHierarchicalSizing_TotalIncludesAllPackages(self):
    data = [
      ('P', '<TOTAL>', 10),
      ('C', 'class1', 3),
      ('C', 'class2', 4),
      ('C', 'class3', 2),
    ]
    nodes = apkanalyzer.UndoHierarchicalSizing(data)
    self.assertEqualLists([
      ('P', '<TOTAL>', 1),
      ('C', 'class1', 3),
      ('C', 'class2', 4),
      ('C', 'class3', 2),
    ], nodes)

  def testUndoHierarchicalSizing_PackageAndClassSameName(self):
    data = [
      ('P', 'name', 4),
      ('C', 'name.Class', 4),
      ('C', 'name', 2),
    ]
    nodes = apkanalyzer.UndoHierarchicalSizing(data)
    self.assertEqualLists([
      ('C', 'name.Class', 4),
      ('C', 'name', 2),
    ], nodes)

  def testLambdaNormalizer_wholeString(self):
    lambda_normalizer = apkanalyzer.LambdaNormalizer()
    name = 'org.-$$Lambda$StackAnimation$Nested1$kjevdDQ8V2zqCrdieLqWLHzk.dex'
    package = name
    expected_outer_class = 'org.StackAnimation'
    expected_name = 'org.StackAnimation$Nested1$$Lambda$0'
    self.assertEqual((expected_outer_class, expected_name),
                     lambda_normalizer.Normalize(package, name))

  def testLambdaNormalizer_wholeString_noDexSuffix(self):
    lambda_normalizer = apkanalyzer.LambdaNormalizer()
    name = 'org.-$$Lambda$StackAnimation$Nested1$kjevdDQ8V2zqCrdieLqWLHzk'
    package = name
    expected_outer_class = 'org.StackAnimation'
    expected_name = 'org.StackAnimation$Nested1$$Lambda$0'
    self.assertEqual((expected_outer_class, expected_name),
                     lambda_normalizer.Normalize(package, name))

  def testLambdaNormalizer_prefix(self):
    lambda_normalizer = apkanalyzer.LambdaNormalizer()
    name = 'org.-$$Lambda$StackAnimation$Nested1$kjevdeLqWLHzk.dex foo bar'
    package = name.split(' ')[0]
    expected_outer_class = 'org.StackAnimation'
    expected_name = 'org.StackAnimation$Nested1$$Lambda$0 foo bar'
    self.assertEqual((expected_outer_class, expected_name),
                     lambda_normalizer.Normalize(package, name))

  def testLambdaNormalizer_prefix_noDexSuffix(self):
    lambda_normalizer = apkanalyzer.LambdaNormalizer()
    name = 'org.-$$Lambda$StackAnimation$Nested1$kjevdeLqWLHzk foo bar'
    package = name.split(' ')[0]
    expected_outer_class = 'org.StackAnimation'
    expected_name = 'org.StackAnimation$Nested1$$Lambda$0 foo bar'
    self.assertEqual((expected_outer_class, expected_name),
                     lambda_normalizer.Normalize(package, name))

  def testLambdaNormalizer_lambdaCounting(self):
    lambda_normalizer = apkanalyzer.LambdaNormalizer()
    name = 'org.-$$Lambda$StackAnimation$Nested1$kjevdDQ8V2zqCrdieLqWLHzk.dex'
    expected_outer_class = 'org.StackAnimation'
    expected_name = 'org.StackAnimation$Nested1$$Lambda$0'
    # Ensure multiple calls to the same class maps to same number.
    self.assertEqual((expected_outer_class, expected_name),
                     lambda_normalizer.Normalize(name, name))
    self.assertEqual((expected_outer_class, expected_name),
                     lambda_normalizer.Normalize(name, name))
    name = 'org.-$$Lambda$StackAnimation$Nested1$kjevdDQ8V2zqCrdieLqWLHzk2.dex'
    expected_name = 'org.StackAnimation$Nested1$$Lambda$1'
    self.assertEqual((expected_outer_class, expected_name),
                     lambda_normalizer.Normalize(name, name))

  def testLambdaNormalizer_lambdaCounting_noDexSuffix(self):
    lambda_normalizer = apkanalyzer.LambdaNormalizer()
    name = 'org.-$$Lambda$StackAnimation$Nested1$kjevdDQ8V2zqCrdieLqWLHzk'
    expected_outer_class = 'org.StackAnimation'
    expected_name = 'org.StackAnimation$Nested1$$Lambda$0'
    # Ensure multiple calls to the same class maps to same number.
    self.assertEqual((expected_outer_class, expected_name),
                     lambda_normalizer.Normalize(name, name))
    self.assertEqual((expected_outer_class, expected_name),
                     lambda_normalizer.Normalize(name, name))
    name = 'org.-$$Lambda$StackAnimation$Nested1$kjevdDQ8V2zqCrdieLqWLHzk2'
    expected_name = 'org.StackAnimation$Nested1$$Lambda$1'
    self.assertEqual((expected_outer_class, expected_name),
                     lambda_normalizer.Normalize(name, name))

  def testLambdaNormalizer_multiSameLine(self):
    lambda_normalizer = apkanalyzer.LambdaNormalizer()
    name = ('org.-$$Lambda$StackAnimation$Nested1$kevdDQ8V2zqCrdieLqWLHzk.dex '
            'org.-$$Lambda$Other$kjevdDQ8V2zqCrdieLqWLHzk.dex.foo bar')
    package = name.split(' ')[0]
    expected_outer_class = 'org.StackAnimation'
    expected_name = ('org.StackAnimation$Nested1$$Lambda$0 '
                     'org.-$$Lambda$Other$kjevdDQ8V2zqCrdieLqWLHzk.dex.foo bar')
    self.assertEqual((expected_outer_class, expected_name),
                     lambda_normalizer.Normalize(package, name))

    name = expected_name
    package = name.split(' ')[1]
    expected_outer_class = 'org.Other'
    expected_name = ('org.StackAnimation$Nested1$$Lambda$0 '
                     'org.Other$$Lambda$0.foo bar')
    self.assertEqual((expected_outer_class, expected_name),
                     lambda_normalizer.Normalize(package, name))

  def testLambdaNormalizer_multiSameLine_noDexSuffix(self):
    lambda_normalizer = apkanalyzer.LambdaNormalizer()
    name = ('org.-$$Lambda$StackAnimation$Nested1$kevdDQ8V2zqCrdieLqWLHzk '
            'org.-$$Lambda$Other$kjevdDQ8V2zqCrdieLqWLHzk bar')
    package = name.split(' ')[0]
    expected_outer_class = 'org.StackAnimation'
    expected_name = ('org.StackAnimation$Nested1$$Lambda$0 '
                     'org.-$$Lambda$Other$kjevdDQ8V2zqCrdieLqWLHzk bar')
    self.assertEqual((expected_outer_class, expected_name),
                     lambda_normalizer.Normalize(package, name))

    name = expected_name
    package = name.split(' ')[1]
    expected_outer_class = 'org.Other'
    expected_name = ('org.StackAnimation$Nested1$$Lambda$0 '
                     'org.Other$$Lambda$0 bar')
    self.assertEqual((expected_outer_class, expected_name),
                     lambda_normalizer.Normalize(package, name))

  def testCreateDexSymbol_normal(self):
    name = ('org.StackAnimation org.ChromeAnimation '
            'createReachTopAnimatorSet(org.StackTab[],float)')
    size = 1
    source_map = {}
    lambda_normalizer = apkanalyzer.LambdaNormalizer()
    symbol = apkanalyzer.CreateDexSymbol(name, size, source_map,
                                         lambda_normalizer)
    self.assertEqual('$APK/org/StackAnimation', symbol.object_path)

  def testCreateDexSymbol_classMerged_noSource(self):
    name = ('org.NewClass org.ChromeAnimation '
            'org.OldClass.createReachTopAnimatorSet(org.StackTab[],float)')
    size = 1
    source_map = {}
    lambda_normalizer = apkanalyzer.LambdaNormalizer()
    symbol = apkanalyzer.CreateDexSymbol(name, size, source_map,
                                         lambda_normalizer)
    self.assertEqual('$APK/org/OldClass', symbol.object_path)

  def testCreateDexSymbol_classMerged_withSource(self):
    name = ('org.NewClass org.ChromeAnimation '
            'org.OldClass.createReachTopAnimatorSet(org.StackTab[],float)')
    size = 1
    source_map = {'org.OldClass': 'old_path.java'}
    lambda_normalizer = apkanalyzer.LambdaNormalizer()
    symbol = apkanalyzer.CreateDexSymbol(name, size, source_map,
                                         lambda_normalizer)
    self.assertEqual('$APK/org/OldClass', symbol.object_path)
    self.assertEqual('old_path.java', symbol.source_path)

  def testCreateDexSymbol_classMerged_field(self):
    name = 'org.NewClass int org.OldClass.createReachTopAnimatorSet'
    size = 1
    source_map = {}
    lambda_normalizer = apkanalyzer.LambdaNormalizer()
    symbol = apkanalyzer.CreateDexSymbol(name, size, source_map,
                                         lambda_normalizer)
    self.assertEqual('$APK/org/OldClass', symbol.object_path)

  def testCreateDexSymbol_total(self):
    name = '<TOTAL>'
    size = 1
    source_map = {}
    lambda_normalizer = apkanalyzer.LambdaNormalizer()
    symbol = apkanalyzer.CreateDexSymbol(name, size, source_map,
                                         lambda_normalizer)
    self.assertIsNone(symbol)


if __name__ == '__main__':
  unittest.main()
