#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors
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

  def assertNormalizedTo(self, class_path, expected_outer_class, expected_name):
    actual_outer_class, actual_name = apkanalyzer.NormalizeLine(
        class_path, class_path)
    self.assertEqual(expected_outer_class, actual_outer_class)
    self.assertEqual(expected_name, actual_name)

  def testNoramlize_internalSyntheticLambda(self):
    self.assertNormalizedTo(
        class_path='pkg.Cls$$InternalSyntheticLambda$3$81073ff626$0',
        expected_outer_class='pkg.Cls',
        expected_name='pkg.Cls$$InternalSyntheticLambda$3')

  def testNoramlize_externalSyntheticLambda(self):
    self.assertNormalizedTo(
        class_path='pkg.AnimatedProgressBar$$ExternalSyntheticLambda0',
        expected_outer_class='pkg.AnimatedProgressBar',
        expected_name=('pkg.AnimatedProgressBar$$ExternalSyntheticLambda0'))

  # Google3 still uses this format.
  def testNoramlize_DesugarLambda(self):
    self.assertNormalizedTo(class_path='pkg.Cls$$Lambda$1',
                            expected_outer_class='pkg.Cls',
                            expected_name='pkg.Cls$$Lambda$1')

  def testNoramlize_apiModelOutline(self):
    self.assertNormalizedTo(
        class_path='pkg.Cls$$ExternalSyntheticApiModelOutline0',
        expected_outer_class='pkg.Cls',
        expected_name='pkg.Cls$$ExternalSyntheticApiModelOutline0')

  def testNoramlize_r8Outline(self):
    self.assertNormalizedTo(class_path='pkg.Cls$$ExternalSyntheticOutline0',
                            expected_outer_class=None,
                            expected_name='pkg.Cls$$ExternalSyntheticOutline0')

  def testNoramlize_externalSyntheticCodegen(self):
    self.assertNormalizedTo(
        class_path='pkg.Cls$$ExternalSyntheticThrowCCEIfNotNull0',
        expected_outer_class=None,
        expected_name=('pkg.Cls$$ExternalSyntheticThrowCCEIfNotNull0'))

    self.assertNormalizedTo(
        class_path='pkg.Cls$$ExternalSyntheticBackportWithForwarding0',
        expected_outer_class=None,
        expected_name=('pkg.Cls$$ExternalSyntheticBackportWithForwarding0'))

  def testNoramlize_externalSyntheticOther(self):
    self.assertNormalizedTo(
        class_path='pkg.Cls$$ExternalSyntheticServiceLoad0',
        expected_outer_class='pkg.Cls',
        expected_name='pkg.Cls$$ExternalSyntheticServiceLoad0',
    )

  def testNoramlize_multiSameLine(self):
    name = ('pkg1.Cls$$InternalSyntheticLambda$3$81073ff626$0 '
            'pkg2.Cls$$InternalSyntheticLambda$0$81073ff626$0 bar')
    outer_class = name.split(' ')[0]
    expected_name = ('pkg1.Cls$$InternalSyntheticLambda$3 '
                     'pkg2.Cls$$InternalSyntheticLambda$0$81073ff626$0 bar')
    self.assertEqual(('pkg1.Cls', expected_name),
                     apkanalyzer.NormalizeLine(outer_class, name))
    name = expected_name
    outer_class = name.split(' ')[1]
    expected_name = ('pkg1.Cls$$InternalSyntheticLambda$3 '
                     'pkg2.Cls$$InternalSyntheticLambda$0 bar')
    self.assertEqual(('pkg2.Cls', expected_name),
                     apkanalyzer.NormalizeLine(outer_class, name))

  def testCreateDexSymbol_normal(self):
    name = ('org.StackAnimation org.ChromeAnimation '
            'createReachTopAnimatorSet(org.StackTab[],float)')
    size = 1
    source_map = {}
    symbol = apkanalyzer.CreateDexSymbol(name, size, source_map)
    self.assertEqual('$APK/org/StackAnimation', symbol.object_path)

  def testCreateDexSymbol_classMerged_noSource(self):
    name = ('org.NewClass org.ChromeAnimation '
            'org.OldClass.createReachTopAnimatorSet(org.StackTab[],float)')
    size = 1
    source_map = {}
    symbol = apkanalyzer.CreateDexSymbol(name, size, source_map)
    self.assertEqual('$APK/org/OldClass', symbol.object_path)

  def testCreateDexSymbol_classMerged_withSource(self):
    name = ('org.NewClass org.ChromeAnimation '
            'org.OldClass.createReachTopAnimatorSet(org.StackTab[],float)')
    size = 1
    source_map = {'org.OldClass': 'old_path.java'}
    symbol = apkanalyzer.CreateDexSymbol(name, size, source_map)
    self.assertEqual('$APK/org/OldClass', symbol.object_path)
    self.assertEqual('old_path.java', symbol.source_path)

  def testCreateDexSymbol_classMerged_field(self):
    name = 'org.NewClass int org.OldClass.createReachTopAnimatorSet'
    size = 1
    source_map = {}
    symbol = apkanalyzer.CreateDexSymbol(name, size, source_map)
    self.assertEqual('$APK/org/OldClass', symbol.object_path)

  def testCreateDexSymbol_total(self):
    name = '<TOTAL>'
    size = 1
    source_map = {}
    symbol = apkanalyzer.CreateDexSymbol(name, size, source_map)
    self.assertIsNone(symbol)


if __name__ == '__main__':
  unittest.main()
