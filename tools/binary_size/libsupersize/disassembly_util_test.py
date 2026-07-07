#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import disassembly_util
import models


class DisassemblyUtilTest(unittest.TestCase):

  def testSelectEvenlySpaced(self):
    self.assertEqual([], disassembly_util._SelectEvenlySpaced([], 3))
    self.assertEqual([1], disassembly_util._SelectEvenlySpaced([1], 3))
    self.assertEqual([1, 2], disassembly_util._SelectEvenlySpaced([1, 2], 3))
    self.assertEqual([1, 2, 3],
                     disassembly_util._SelectEvenlySpaced([1, 2, 3], 3))
    self.assertEqual([2, 3, 4],
                     disassembly_util._SelectEvenlySpaced([1, 2, 3, 4], 3))
    self.assertEqual([2, 4, 5],
                     disassembly_util._SelectEvenlySpaced([1, 2, 3, 4, 5], 3))

  def testSelectSymbols(self):

    def make_delta(name, before_size, after_size, source_path=None):
      before = None
      if before_size:
        before = models.Symbol(models.SECTION_TEXT,
                               size_without_padding=before_size,
                               name=name)
        before.source_path = source_path
      after = None
      if after_size:
        after = models.Symbol(models.SECTION_TEXT,
                              size_without_padding=after_size,
                              name=name)
        after.source_path = source_path
      return models.DeltaSymbol(before, after)

    # Empty candidates
    self.assertEqual([],
                     disassembly_util.SampleSymbols(
                         models.DeltaSymbolGroup([], name='test')))

    # Create a set of candidates
    symbols = []
    # 10 added in Foo.java (large)
    for i in range(10):
      symbols.append(make_delta(f'added_{i}', None, 100 + i, 'Foo.java'))
    # 10 removed in Bar.java (large)
    for i in range(10):
      symbols.append(make_delta(f'removed_{i}', 100 + i, None, 'Bar.java'))
    # 10 changed in Baz.java (large)
    for i in range(10):
      symbols.append(make_delta(f'changed_{i}', 100, 150 + i, 'Baz.java'))
    # 5 changed in Minior.java (small, diff = 1)
    for i in range(5):
      symbols.append(make_delta(f'minior_{i}', 100, 101, 'Minior.java'))
    # 1 changed in NewFile.java (small, diff = 1)
    symbols.append(make_delta('new_0', 100, 101, 'NewFile.java'))

    candidates = models.DeltaSymbolGroup(symbols, name='test')

    # 1) Test without changed_files
    selected = disassembly_util.SampleSymbols(candidates)

    # Verify we have at least 2 of each status (since we take 2 biggest)
    # and some from evenly spaced.
    added = [s for s in selected if s.diff_status == models.DIFF_STATUS_ADDED]
    removed = [
        s for s in selected if s.diff_status == models.DIFF_STATUS_REMOVED
    ]
    changed = [
        s for s in selected if s.diff_status == models.DIFF_STATUS_CHANGED
    ]
    self.assertGreaterEqual(len(added), 2)
    self.assertGreaterEqual(len(removed), 2)
    self.assertGreaterEqual(len(changed), 2)

    # Minior.java has small changes, so it should NOT have 3 symbols selected
    # by default
    minior_symbols = [s for s in selected if s.source_path == 'Minior.java']
    self.assertLess(len(minior_symbols), 3)

    # 2) Test with changed_files=['Minior.java']
    # Since we need at least 8 from changed files and Minior only has 5, all 5
    # must be selected.
    selected_with_files = disassembly_util.SampleSymbols(
        candidates, changed_files=['Minior.java'])
    minior_symbols_with_files = [
        s for s in selected_with_files if s.source_path == 'Minior.java'
    ]
    self.assertEqual(len(minior_symbols_with_files), 5)

    # 3) Test scoring system: NewFile.java has a tiny symbol and hasn't
    # contributed yet, whereas Baz.java has already contributed large symbols.
    # NewFile.java should be prioritized.
    selected_scoring = disassembly_util.SampleSymbols(
        candidates, changed_files=['Baz.java', 'NewFile.java'])
    newfile_symbols = [
        s for s in selected_scoring if s.source_path == 'NewFile.java'
    ]
    self.assertEqual(len(newfile_symbols), 1)

    # If we specify a file that has no candidates, it shouldn't crash
    selected_empty_file = disassembly_util.SampleSymbols(
        candidates, changed_files=['NonExistent.java'])
    self.assertGreater(len(selected_empty_file), 0)

  def testCreateUnifiedDiff(self):
    before = ['line1\n', 'line2\n']
    after = ['line1\n', 'line3\n']
    diff = disassembly_util.CreateUnifiedDiff('test_symbol', before, after)
    self.assertIn('--- before/test_symbol', diff)
    self.assertIn('+++ after/test_symbol', diff)
    self.assertIn('-line2', diff)
    self.assertIn('+line3', diff)


if __name__ == '__main__':
  unittest.main()
