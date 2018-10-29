#!/usr/bin/env python
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


if __name__ == '__main__':
  unittest.main()
