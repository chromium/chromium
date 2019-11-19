#!/usr/bin/env vpython
# Copyright 2018 The Chromium Authors. All rights reserved.
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

  def testClusteringDistancesForCallGraph(self):
    c = cluster.Clustering()
    callerA = cluster.CallerInfo(caller_symbol='a', count=1)
    callerB = cluster.CallerInfo(caller_symbol='b', count=2)
    callerC = cluster.CallerInfo(caller_symbol='c', count=3)
    callerD = cluster.CallerInfo(caller_symbol='d', count=100)
    callerE = cluster.CallerInfo(caller_symbol='e', count=200)

    calleeA = cluster.CalleeInfo(index=4, callee_symbol='a', misses=0,
                                 caller_and_count=[])
    calleeB = cluster.CalleeInfo(index=8, callee_symbol='b', misses=1,
                                 caller_and_count=[callerA])
    calleeC = cluster.CalleeInfo(index=12, callee_symbol='c', misses=1,
                                 caller_and_count=[callerA, callerE])
    calleeD = cluster.CalleeInfo(index=20, callee_symbol='d', misses=1,
                                 caller_and_count=[callerB, callerC, callerE])
    calleeF = cluster.CalleeInfo(index=28, callee_symbol='f', misses=10,
                                 caller_and_count=[callerD])
    process1 = [calleeA, calleeB, calleeC, calleeD]
    process2 = [calleeA, calleeB, calleeC, calleeD, calleeF]
    call_graph = [process1, process2]
    whitelist = ['e', 'g', 'h', 'k', 'l']
    c.AddSymbolCallGraph(call_graph, whitelist)
    distances = {}
    for n in c._neighbors:
      self.assertFalse((n.src, n.dst) in distances)
      distances[(n.src, n.dst)] = n.dist
    self.assertEqual(5, len(distances))
    self.assertEquals(-2, distances[('a', 'b')])
    self.assertEquals(-2, distances[('a', 'c')])
    self.assertEquals(-4, distances[('b', 'd')])
    self.assertEquals(-6, distances[('c', 'd')])
    self.assertEquals(-100, distances[('d', 'f')])
    self.assertEquals(list('abcdf'), c.ClusterToList())

  def testClusterOffsetsFromCallGraph(self):
    process1 = ('{"call_graph": [ {'
                  '"callee_offset": "1000",'
                  '"caller_and_count": [ {'
                    '"caller_offset": "0",'
                    '"count": "2"'
                  '} ],'
                  '"index": "61496"'
                '}, {'
                  '"callee_offset": "7000",'
                  '"caller_and_count": [ {'
                    '"caller_offset": "1000",'
                    '"count": "2"'
                  '}, {'
                    '"caller_offset": "7500",'
                    '"count": "100"'
                  '} ],'
                  '"index": "61500"'
                '}, {'
                  '"callee_offset": "6000",'
                  '"caller_and_count": [ {'
                    '"caller_offset": "1000",'
                    '"count": "4"'
                  '}, {'
                    '"caller_offset": "7000",'
                    '"count": "3"'
                  '}, {'
                    '"caller_offset": "7500",'
                    '"count": "2"'
                  '}, {'
                    '"caller_offset": "0",'
                    '"count": "3"'
                  '} ],'
                  '"index": "47860"'
                '}, {'
                  '"callee_offset": "3000",'
                  '"caller_and_count": [ {'
                    '"caller_offset": "6000",'
                    '"count": "11"'
                  '} ],'
                  '"index": "47900"'
                '} ],'
                '"total_calls_count": "127"'
                '}')

    process2 = ('{"call_graph": [ {'
                  '"callee_offset": "1000",'
                  '"caller_and_count": [ {'
                    '"caller_offset": "0",'
                    '"count": "2"'
                  '} ],'
                  '"index": "61496"'
                  '}, {'
                  '"callee_offset": "5000",'
                  '"caller_and_count": [ {'
                    '"caller_offset": "1000",'
                    '"count": "20"'
                  '}, {'
                    '"caller_offset": "5000",'
                    '"count": "100"'
                  '}, {'
                    '"caller_offset": "3000",'
                    '"count": "40"'
                  '} ],'
                  '"index": "61500"'
                '}, {'
                  '"callee_offset": "3000",'
                  '"caller_and_count": [ {'
                    '"caller_offset": "5000",'
                    '"count": "10"'
                  '}, {'
                    '"caller_offset": "0",'
                    '"count": "10"'
                  '} ],'
                  '"index": "47860"'
                '} ],'
                '"total_calls_count": "182"'
                '}')

    process3 = ('{"call_graph": [ {'
                  '"callee_offset": "8000",'
                  '"caller_and_count": [ {'
                    '"caller_offset": "0",'
                    '"count": "5"'
                  '} ],'
                  '"index": "61496"'
                  '}, {'
                  '"callee_offset": "2000",'
                  '"caller_and_count": [ {'
                    '"caller_offset": "8000",'
                    '"count": "100"'
                  '} ],'
                  '"index": "61500"'
                  '}, {'
                  '"callee_offset": "4000",'
                  '"caller_and_count": [ {'
                    '"caller_offset": "8000",'
                    '"count": "20"'
                  '} ],'
                  '"index": "61504"'
                  '}, {'
                  '"callee_offset": "9000",'
                  '"caller_and_count": [ {'
                    '"caller_offset": "8000",'
                    '"count": "50"'
                  '} ],'
                  '"index": "61512"'
                  '}, {'
                  '"callee_offset": "7000",'
                  '"caller_and_count": [ {'
                    '"caller_offset": "2000",'
                    '"count": "15"'
                  '}, {'
                    '"caller_offset": "4000",'
                    '"count": "20"'
                  '}, {'
                    '"caller_offset": "9000",'
                    '"count": "80"'
                  '}, {'
                    '"caller_offset": "0",'
                    '"count": "400"'
                  '} ],'
                  '"index": "61516"'
                  '} ],'
                  '"total_calls_count": "690"'
                  '}')

    process4 = ('{"call_graph": [ {'
                '"callee_offset": "8000",'
                '"caller_and_count": [ {'
                  '"caller_offset": "0",'
                  '"count": "10"'
                '} ],'
                '"index": "61496"'
                '}, {'
                '"callee_offset": "2000",'
                '"caller_and_count": [ {'
                  '"caller_offset": "8000",'
                  '"count": "100"'
                '} ],'
                '"index": "61500"'
                '}, {'
                '"callee_offset": "6000",'
                '"caller_and_count": [ {'
                  '"caller_offset": "7000",'
                  '"count": "10"'
                '} , {'
                  '"caller_offset": "7500",'
                  '"count": "2"'
                '} ],'
                '"index": "61504"'
                '}, {'
                '"callee_offset": "7000",'
                '"caller_and_count": [ {'
                  '"caller_offset": "8000",'
                  '"count": "300"'
                '}, {'
                  '"caller_offset": "7500",'
                  '"count": "100"'
                '}, {'
                  '"caller_offset": "2000",'
                  '"count": "15"'
                '}, {'
                  '"caller_offset": "0",'
                  '"count": "50"'
                '} ],'
                '"index": "61516"'
                '} ],'
                '"total_calls_count": "587"'
                '}')

    processor = TestSymbolOffsetProcessor([
        SimpleTestSymbol('linker_script_start_of_text', 0, 0),
        SimpleTestSymbol('1', 1000, 999),
        SimpleTestSymbol('2', 2000, 999),
        SimpleTestSymbol('3', 3000, 999),
        SimpleTestSymbol('4', 4000, 16),
        SimpleTestSymbol('5', 5000, 16),
        SimpleTestSymbol('6', 6000, 999),
        SimpleTestSymbol('7', 7000, 16),
        SimpleTestSymbol('8', 7100, 0),  # whitelist
        SimpleTestSymbol('9', 8000, 999),
        SimpleTestSymbol('10', 9000, 16)])
    mgr = TestProfileManager({
        ProfileFile(40, 0, 'renderer'): json.loads(process1),
        ProfileFile(50, 1, 'renderer'): json.loads(process2),
        ProfileFile(51, 0, 'browser'): json.loads(process3),
        ProfileFile(51, 1, 'gpu-process'): json.loads(process4)})
    syms = cluster.ClusterOffsets(mgr, processor, limit_cluster_size=False,
                                  call_graph=True)
    self.assertListEqual(['7', '6', '1', '5', '3', '9', '2', '10', '4'], syms)



if __name__ == "__main__":
  unittest.main()
