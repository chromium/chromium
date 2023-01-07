#!/usr/bin/env vpython3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for process_profiles.py."""

import collections
import unittest

import process_profiles

from test_utils import (ProfileFile,
                        SimpleTestSymbol,
                        TestSymbolOffsetProcessor,
                        TestProfileManager)

class ProcessProfilesTestCase(unittest.TestCase):
  START_SYMBOL = 'linker_script_start_of_text'

  def setUp(self):
    self.symbol_0 = SimpleTestSymbol(self.START_SYMBOL, 0, 0)
    self.symbol_1 = SimpleTestSymbol('1', 6, 16)
    self.symbol_2 = SimpleTestSymbol('2', 32, 8)
    self.symbol_3 = SimpleTestSymbol('3', 40, 12)
    self.offset_to_symbol_info = (
        [None] * 3 + [self.symbol_1] * 8 + [None] * 5 + [self.symbol_2] * 4 +
        [self.symbol_3] * 6)
    self.symbol_infos = [self.symbol_0, self.symbol_1,
                         self.symbol_2, self.symbol_3]
    self._file_counter = 0

  def MakeAnnotatedOffset(self, offset, counts):
    ao = process_profiles.ProfileManager.AnnotatedOffset(offset)
    ao._count = counts
    return ao

  def testGetOffsetToSymbolInfo(self):
    processor = TestSymbolOffsetProcessor(self.symbol_infos)
    self.assertListEqual(self.offset_to_symbol_info,
                         processor.GetDumpOffsetToSymbolInfo())

  def testOverlappingSymbols(self):
    symbol_1 = SimpleTestSymbol(self.START_SYMBOL, 6, 8)
    symbol_2 = SimpleTestSymbol('2', 10, 10)
    processor = TestSymbolOffsetProcessor([symbol_1, symbol_2])
    self.assertListEqual([symbol_1] * 4 + [symbol_2] * 3,
                         processor.GetDumpOffsetToSymbolInfo())

  def testSymbolsBeforeStart(self):
    self.symbol_infos = [SimpleTestSymbol(s.name, s.offset + 8, s.size)
                         for s in self.symbol_infos]
    self.symbol_infos.append(SimpleTestSymbol('early', 0, 4))
    processor = TestSymbolOffsetProcessor(self.symbol_infos)
    self.assertRaises(AssertionError, processor.GetDumpOffsetToSymbolInfo)

  def testGetReachedOffsetsFromDump(self):
    processor = TestSymbolOffsetProcessor(self.symbol_infos)
    # 2 hits for symbol_1, 0 for symbol_2, 1 for symbol_3
    dump = [8, 12, 48]
    reached = processor.GetReachedOffsetsFromDump(dump)
    self.assertListEqual([self.symbol_1.offset, self.symbol_3.offset], reached)
    # Ordering matters, no repetitions
    dump = [48, 12, 8, 12, 8, 16]
    reached = processor.GetReachedOffsetsFromDump(dump)
    self.assertListEqual([self.symbol_3.offset, self.symbol_1.offset], reached)

  def testSymbolNameToPrimary(self):
    symbol_infos = [SimpleTestSymbol('1', 8, 16),
                    SimpleTestSymbol('AnAlias', 8, 16),
                    SimpleTestSymbol('Another', 40, 16)]
    processor = TestSymbolOffsetProcessor(symbol_infos)
    self.assertDictEqual({8: symbol_infos[0],
                          40: symbol_infos[2]}, processor.OffsetToPrimaryMap())

  def testGetOrderedSymbols(self):
    processor = TestSymbolOffsetProcessor(self.symbol_infos)
    self.assertListEqual(['1', '3', self.START_SYMBOL],
                         processor.GetOrderedSymbols([7, 41, 5, 0]))

  def testOffsetToSymbolsMap(self):
    symbol_infos = [SimpleTestSymbol('1', 8, 16),
                    SimpleTestSymbol('AnAlias', 8, 16),
                    SimpleTestSymbol('Another', 40, 16)]
    processor = TestSymbolOffsetProcessor(symbol_infos)
    self.assertDictEqual({8: [symbol_infos[0], symbol_infos[1]],
                          40: [symbol_infos[2]]},
                         processor.OffsetToSymbolsMap())

  def testPrimarySizeMismatch(self):
    symbol_infos = [SimpleTestSymbol('1', 8, 16),
                    SimpleTestSymbol('AnAlias', 8, 32)]
    processor = TestSymbolOffsetProcessor(symbol_infos)
    self.assertRaises(AssertionError, processor.OffsetToPrimaryMap)
    symbol_infos = [SimpleTestSymbol('1', 8, 0),
                    SimpleTestSymbol('2', 8, 32),
                    SimpleTestSymbol('3', 8, 32),
                    SimpleTestSymbol('4', 8, 0),]
    processor = TestSymbolOffsetProcessor(symbol_infos)
    self.assertDictEqual({8: symbol_infos[1]}, processor.OffsetToPrimaryMap())

  def testMatchSymbols(self):
    symbols = [SimpleTestSymbol('W', 30, 10),
               SimpleTestSymbol('Y', 60, 5),
               SimpleTestSymbol('X', 100, 10)]
    processor = TestSymbolOffsetProcessor(symbols)
    self.assertListEqual(sorted(symbols[1:3]),
                         processor.MatchSymbolNames(['Y', 'X']))

  def testSymbolsSize(self):
    symbols = [SimpleTestSymbol('W', 10, 1),
               SimpleTestSymbol('X', 20, 2),
               SimpleTestSymbol('Y', 30, 4),
               SimpleTestSymbol('Z', 40, 8)]
    processor = TestSymbolOffsetProcessor(symbols)
    self.assertEqual(13, processor.SymbolsSize(['W', 'Y', 'Z']))

  def testMedian(self):
    self.assertEquals(None, process_profiles._Median([]))
    self.assertEquals(5, process_profiles._Median([5]))
    self.assertEquals(5, process_profiles._Median([1, 5, 20]))
    self.assertEquals(5, process_profiles._Median([4, 6]))
    self.assertEquals(5, process_profiles._Median([1, 4, 6, 100]))
    self.assertEquals(5, process_profiles._Median([1, 4, 5, 6, 100]))

  def testRunGroups(self):
    files = [ProfileFile(40, 0), ProfileFile(100, 0),
             ProfileFile(200, 1), ProfileFile(35, 1),
             ProfileFile(42, 0), ProfileFile(95, 0)]
    mgr = process_profiles.ProfileManager(files)
    mgr._ComputeRunGroups()
    self.assertEquals(3, len(mgr._run_groups))
    self.assertEquals(3, len(mgr._run_groups[0].Filenames()))
    self.assertEquals(2, len(mgr._run_groups[1].Filenames()))
    self.assertEquals(1, len(mgr._run_groups[2].Filenames()))
    self.assertTrue(files[0] in mgr._run_groups[0].Filenames())
    self.assertTrue(files[3] in mgr._run_groups[0].Filenames())
    self.assertTrue(files[4] in mgr._run_groups[0].Filenames())
    self.assertTrue(files[1] in mgr._run_groups[1].Filenames())
    self.assertTrue(files[5] in mgr._run_groups[1].Filenames())
    self.assertTrue(files[2] in mgr._run_groups[2].Filenames())

  def testRunGroupSanity(self):
    files = []
    # Generate 20 sets of files in groups separated by 60s.
    for ts_base in range(0, 20):
      ts = ts_base * 60
      files.extend([ProfileFile(ts, 0, 'browser'),
                    ProfileFile(ts + 1, 0, 'renderer'),
                    ProfileFile(ts + 2, 1, 'browser'),
                    ProfileFile(ts + 3, 0, 'gpu'),
                    ProfileFile(ts + 2, 1, 'renderer'),
                    ProfileFile(ts + 5, 1, 'gpu')])
    # The following call should not assert.
    process_profiles.ProfileManager(files)._ComputeRunGroups()

    files.extend([
        ProfileFile(20 * 60, 0, 'browser'),
        ProfileFile(20 * 60 + 2, 1, 'renderer'),
        ProfileFile(21 * 60, 0, 'browser')
    ] + [ProfileFile(22 * 60, 0, 'renderer') for _ in range(0, 10)])

    self.assertRaises(AssertionError,
                      process_profiles.ProfileManager(files)._ComputeRunGroups)

  def testReadOffsets(self):
    mgr = TestProfileManager({
        ProfileFile(30, 0): [1, 3, 5, 7],
        ProfileFile(40, 1): [8, 10],
        ProfileFile(50, 0): [13, 15]})
    self.assertListEqual([1, 3, 5, 7, 8, 10, 13, 15],
                         mgr.GetMergedOffsets())
    self.assertListEqual([8, 10], mgr.GetMergedOffsets(1))
    self.assertListEqual([], mgr.GetMergedOffsets(2))

  def testRunGroupOffsets(self):
    mgr = TestProfileManager({
        ProfileFile(30, 0): [1, 2, 3, 4],
        ProfileFile(150, 0): [9, 11, 13],
        ProfileFile(40, 1): [5, 6, 7]})
    offsets_list = mgr.GetRunGroupOffsets()
    self.assertEquals(2, len(offsets_list))
    self.assertListEqual([1, 2, 3, 4, 5, 6, 7], offsets_list[0])
    self.assertListEqual([9, 11, 13], offsets_list[1])
    offsets_list = mgr.GetRunGroupOffsets(0)
    self.assertEquals(2, len(offsets_list))
    self.assertListEqual([1, 2, 3, 4], offsets_list[0])
    self.assertListEqual([9, 11, 13], offsets_list[1])
    offsets_list = mgr.GetRunGroupOffsets(1)
    self.assertEquals(2, len(offsets_list))
    self.assertListEqual([5, 6, 7], offsets_list[0])
    self.assertListEqual([], offsets_list[1])

  def testSorted(self):
    # The fact that the ProfileManager sorts by filename is implicit in the
    # other tests. It is tested explicitly here.
    mgr = TestProfileManager({
        ProfileFile(40, 0): [1, 2, 3, 4],
        ProfileFile(150, 0): [9, 11, 13],
        ProfileFile(30, 1): [5, 6, 7]})
    offsets_list = mgr.GetRunGroupOffsets()
    self.assertEquals(2, len(offsets_list))
    self.assertListEqual([5, 6, 7, 1, 2, 3, 4], offsets_list[0])

  def testPhases(self):
    mgr = TestProfileManager({
        ProfileFile(40, 0): [],
        ProfileFile(150, 0): [],
        ProfileFile(30, 1): [],
        ProfileFile(30, 2): [],
        ProfileFile(30, 0): []})
    self.assertEquals(set([0,1,2]), mgr.GetPhases())

  def testGetAnnotatedOffsets(self):
    mgr = TestProfileManager({
        ProfileFile(40, 0, ''): [1, 2, 3],
        ProfileFile(50, 1, ''): [3, 4, 5],
        ProfileFile(51, 0, 'renderer'): [2, 3, 6],
        ProfileFile(51, 1, 'gpu-process'): [6, 7],
        ProfileFile(70, 0, ''): [2, 8, 9],
        ProfileFile(70, 1, ''): [9]})
    offsets = list(mgr.GetAnnotatedOffsets())
    self.assertListEqual([
        self.MakeAnnotatedOffset(1, {(0, 'browser'): 1}),
        self.MakeAnnotatedOffset(2, {(0, 'browser'): 2,
                                     (0, 'renderer'): 1}),
        self.MakeAnnotatedOffset(3, {(0, 'browser'): 1,
                                     (1, 'browser'): 1,
                                     (0, 'renderer'): 1}),
        self.MakeAnnotatedOffset(4, {(1, 'browser'): 1}),
        self.MakeAnnotatedOffset(5, {(1, 'browser'): 1}),
        self.MakeAnnotatedOffset(6, {(0, 'renderer'): 1,
                                     (1, 'gpu-process'): 1}),
        self.MakeAnnotatedOffset(7, {(1, 'gpu-process'): 1}),
        self.MakeAnnotatedOffset(8, {(0, 'browser'): 1}),
        self.MakeAnnotatedOffset(9, {(0, 'browser'): 1,
                                     (1, 'browser'): 1})],
                         offsets)
    self.assertListEqual(['browser', 'renderer'],
                         sorted(offsets[1].Processes()))
    self.assertListEqual(['browser'], list(offsets[0].Processes()))
    self.assertListEqual([0], list(offsets[1].Phases()))
    self.assertListEqual([0, 1], sorted(offsets[2].Phases()))
    self.assertListEqual([0, 1], sorted(mgr.GetPhases()))


if __name__ == '__main__':
  unittest.main()
