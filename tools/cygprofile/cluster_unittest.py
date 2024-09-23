#!/usr/bin/env vpython3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for cluster.py."""

import unittest
import json

import cluster
import process_profiles
from test_utils import (ProfileFile,
                        SimpleTestSymbol,
                        TestProfileManager,
                        TestSymbolOffsetProcessor)


class ClusteringTestCase(unittest.TestCase):
  def testClusterOf(self):
    clstr = cluster.Clustering()
    c = clstr.ClusterOf('a')
    self.assertEqual(['a'], c.syms)
    c = clstr._MakeCluster(['a', 'b', 'c'])
    self.assertEqual(c, clstr.ClusterOf('a'))
    self.assertEqual(c, clstr.ClusterOf('b'))
    self.assertEqual(c, clstr.ClusterOf('c'))

  def testClusterCombine(self):
    clstr = cluster.Clustering()
    x = clstr._MakeCluster(['a', 'b'])
    self.assertEqual(x, clstr.ClusterOf('a'))
    self.assertEqual(x, clstr.ClusterOf('b'))

    y = clstr._MakeCluster(['c'])
    self.assertEqual(y, clstr.ClusterOf('c'))

    z = clstr.Combine(y, x)
    self.assertEqual(['c', 'a', 'b'], z.syms)
    self.assertEqual(z, clstr.ClusterOf('a'))
    self.assertEqual(z, clstr.ClusterOf('b'))
    self.assertEqual(z, clstr.ClusterOf('c'))

  def testClusteringDistances(self):
    c = cluster.Clustering()
    c.NEIGHBOR_DISTANCE = 3
    c.AddSymbolLists([list('abcd'), list('acbe'), list('bacf'),
                      list('badf'), list('baef')])
    distances = {}
    for n in c._neighbors:
      self.assertFalse((n.src, n.dst) in distances)
      distances[(n.src, n.dst)] = n.dist
    self.assertEqual(13, len(distances))
    self.assertEqual((2 + 1 + 1 + 2000) / 5., distances[('a', 'c')])
    self.assertEqual((1 + 4000) / 5., distances[('a', 'd')])
    self.assertEqual((1 + 4000) / 5., distances[('a', 'e')])
    self.assertEqual((2 + 2 + 2 + 2000) / 5., distances[('a', 'f')])
    self.assertEqual(0, distances[('b', 'a')])
    self.assertEqual((1 + -1 + 2 + 2000) / 5., distances[('b', 'c')])
    self.assertTrue(('b', 'd') in distances)
    self.assertTrue(('b', 'e') in distances)
    self.assertTrue(('c', 'd') in distances)
    self.assertTrue(('c', 'e') in distances)
    self.assertTrue(('c', 'f') in distances)
    self.assertTrue(('d', 'f') in distances)
    self.assertTrue(('e', 'f') in distances)

  def testClusterToList(self):
    c = cluster.Clustering()
    c.NEIGHBOR_DISTANCE = 3
    c.AddSymbolLists([list('abcd'), list('acbe'), list('bacf'),
                      list('badf'), list('baef')])
    self.assertEqual(list('bacfed'), c.ClusterToList())

  def testClusterOneList(self):
    c = cluster.Clustering()
    c.NEIGHBOR_DISTANCE = 3
    c.AddSymbolLists([list('fedcba')])
    self.assertEqual(list('fedcba'), c.ClusterToList())

  def testClusterShortList(self):
    c = cluster.Clustering()
    c.NEIGHBOR_DISTANCE = 3
    c.AddSymbolLists([list('ab')])
    self.assertEqual(list('ab'), c.ClusterToList())

  def testClusterReallyShortList(self):
    c = cluster.Clustering()
    c.NEIGHBOR_DISTANCE = 3
    c.AddSymbolLists([list('a')])
    self.assertEqual([], c.ClusterToList())

  def testSizedClusterToList(self):
    c = cluster.Clustering()
    c.NEIGHBOR_DISTANCE = 3
    c.MAX_CLUSTER_SIZE = 1  # Will supress all clusters
    size_map = {'a': 3,
                'b': 4,
                'c': 5,
                'd': 6,
                'e': 7,
                'f': 8}
    c.AddSymbolLists([list('abcd'), list('acbe'), list('bacf'),
                      list('badf'), list('baef')])
    self.assertEqual(list('fedcba'), c.ClusterToList(size_map))

  def testClusterOffsets(self):
    processor = TestSymbolOffsetProcessor([
        SimpleTestSymbol('linker_script_start_of_text', 0, 0),
        SimpleTestSymbol('1', 1000, 999),
        SimpleTestSymbol('2', 2000, 999),
        SimpleTestSymbol('3', 3000, 999),
        SimpleTestSymbol('4', 4000, 16),
        SimpleTestSymbol('5', 5000, 16),
        SimpleTestSymbol('6', 6000, 999),
        SimpleTestSymbol('7', 7000, 16),
        SimpleTestSymbol('8', 8000, 999),
        SimpleTestSymbol('9', 9000, 16),
    ])
    mgr = TestProfileManager({
        ProfileFile(40, 0, ''): [1000, 2000, 3000],
        ProfileFile(50, 1, ''): [3000, 4000, 5000],
        ProfileFile(51, 0, 'renderer'): [2000, 3000, 6000],
        ProfileFile(51, 1, 'gpu-process'): [6000, 7000],
        ProfileFile(70, 0, ''): [1000, 2000, 6000, 8000, 9000],
        ProfileFile(70, 1, ''): [9000, 5000, 3000]})
    syms = cluster.ClusterOffsets(mgr, processor, limit_cluster_size=False)
    self.assertListEqual(list('236148957'), syms)

    syms = cluster.ClusterOffsets(mgr, processor, limit_cluster_size=True)
    self.assertListEqual(list('236489517'), syms)


if __name__ == "__main__":
  unittest.main()
