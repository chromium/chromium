#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import diff
import models


def _MakeSym(section, size, path, name=None):
  if name is None:
    # Trailing letter is important since diffing trims numbers.
    name = '{}_{}A'.format(section[1:], size)
  return models.Symbol(
      section,
      size,
      full_name=name,
      template_name=name,
      name=name,
      object_path=path)


def _SetName(symbol, full_name, name=None):
  if name is None:
    name = full_name
  symbol.full_name = full_name
  symbol.template_name = full_name
  symbol.name = name


def _CreateSizeInfo(aliases=None):
  section_sizes = {'.text': 100, '.bss': 40}
  TEXT = models.SECTION_TEXT
  symbols = [
      _MakeSym(models.SECTION_DEX_METHOD, 10, 'a', 'com.Foo#bar()'),
      _MakeSym(TEXT, 20, 'a', '.Lfoo'),
      _MakeSym(TEXT, 30, 'b'),
      _MakeSym(TEXT, 40, 'b'),
      _MakeSym(TEXT, 50, 'b'),
      _MakeSym(TEXT, 60, ''),
  ]
  if aliases:
    for tup in aliases:
      syms = symbols[tup[0]:tup[1]]
      for sym in syms:
        sym.aliases = syms
  return models.SizeInfo(section_sizes, symbols)


class DiffTest(unittest.TestCase):

  def testIdentity(self):
    size_info1 = _CreateSizeInfo()
    size_info2 = _CreateSizeInfo()
    d = diff.Diff(size_info1, size_info2)
    self.assertEquals((0, 0, 0), d.raw_symbols.CountsByDiffStatus()[1:])
    self.assertEquals(0, d.raw_symbols.size)
    self.assertEquals(0, d.raw_symbols.padding)

  def testSimple_Add(self):
    size_info1 = _CreateSizeInfo()
    size_info2 = _CreateSizeInfo()
    size_info1.raw_symbols -= [size_info1.raw_symbols[0]]
    d = diff.Diff(size_info1, size_info2)
    self.assertEquals((0, 1, 0), d.raw_symbols.CountsByDiffStatus()[1:])
    self.assertEquals(10, d.raw_symbols.size)
    self.assertEquals(0, d.raw_symbols.padding)

  def testSimple_Delete(self):
    size_info1 = _CreateSizeInfo()
    size_info2 = _CreateSizeInfo()
    size_info2.raw_symbols -= [size_info2.raw_symbols[0]]
    d = diff.Diff(size_info1, size_info2)
    self.assertEquals((0, 0, 1), d.raw_symbols.CountsByDiffStatus()[1:])
    self.assertEquals(-10, d.raw_symbols.size)
    self.assertEquals(0, d.raw_symbols.padding)

  def testSimple_Change(self):
    size_info1 = _CreateSizeInfo()
    size_info2 = _CreateSizeInfo()
    size_info2.raw_symbols[0].size += 11
    size_info2.raw_symbols[0].padding += 20
    size_info2.raw_symbols[-1].size += 11
    d = diff.Diff(size_info1, size_info2)
    self.assertEquals((2, 1, 0), d.raw_symbols.CountsByDiffStatus()[1:])
    self.assertEquals(22, d.raw_symbols.size)
    self.assertEquals(20, d.raw_symbols.padding)

  def testDontMatchAcrossSections(self):
    size_info1 = _CreateSizeInfo()
    size_info1.raw_symbols += [
        _MakeSym(models.SECTION_TEXT, 11, 'asdf', name='Hello'),
    ]
    size_info2 = _CreateSizeInfo()
    size_info2.raw_symbols += [
        _MakeSym(models.SECTION_RODATA, 11, 'asdf', name='Hello'),
    ]
    d = diff.Diff(size_info1, size_info2)
    self.assertEquals((0, 1, 1), d.raw_symbols.CountsByDiffStatus()[1:])
    self.assertEquals(0, d.raw_symbols.size)

  def testAliases_Remove(self):
    size_info1 = _CreateSizeInfo(aliases=[(0, 3)])
    size_info2 = _CreateSizeInfo(aliases=[(0, 2)])
    d = diff.Diff(size_info1, size_info2)
    # Aliases cause all sizes to change.
    self.assertEquals((3, 0, 0), d.raw_symbols.CountsByDiffStatus()[1:])
    self.assertEquals(0, d.raw_symbols.size)

  def testAliases_Add(self):
    size_info1 = _CreateSizeInfo(aliases=[(0, 2)])
    size_info2 = _CreateSizeInfo(aliases=[(0, 3)])
    d = diff.Diff(size_info1, size_info2)
    # Aliases cause all sizes to change.
    self.assertEquals((3, 0, 0), d.raw_symbols.CountsByDiffStatus()[1:])
    self.assertEquals(0, d.raw_symbols.size)

  def testAliases_ChangeGroup(self):
    size_info1 = _CreateSizeInfo(aliases=[(0, 2), (2, 5)])
    size_info2 = _CreateSizeInfo(aliases=[(0, 3), (3, 5)])
    d = diff.Diff(size_info1, size_info2)
    # Aliases cause all sizes to change.
    self.assertEquals((4, 0, 0), d.raw_symbols.CountsByDiffStatus()[1:])
    self.assertEquals(0, d.raw_symbols.size)

  def testStarSymbolNormalization(self):
    size_info1 = _CreateSizeInfo()
    _SetName(size_info1.raw_symbols[0], '* symbol gap 1 (end of section)')
    size_info2 = _CreateSizeInfo()
    _SetName(size_info2.raw_symbols[0], '* symbol gap 2 (end of section)')
    d = diff.Diff(size_info1, size_info2)
    self.assertEquals((0, 0, 0), d.raw_symbols.CountsByDiffStatus()[1:])
    self.assertEquals(0, d.raw_symbols.size)

  def testNumberNormalization(self):
    TEXT = models.SECTION_TEXT
    size_info1 = _CreateSizeInfo()
    size_info1.raw_symbols += [
        _MakeSym(TEXT, 11, 'a', name='.L__unnamed_1193'),
        _MakeSym(TEXT, 22, 'a', name='.L__unnamed_1194'),
        _MakeSym(TEXT, 33, 'a', name='SingleCategoryPreferences$3#this$0'),
        _MakeSym(TEXT, 44, 'a', name='.L.ref.tmp.2'),
    ]
    size_info2 = _CreateSizeInfo()
    size_info2.raw_symbols += [
        _MakeSym(TEXT, 11, 'a', name='.L__unnamed_2194'),
        _MakeSym(TEXT, 22, 'a', name='.L__unnamed_2195'),
        _MakeSym(TEXT, 33, 'a', name='SingleCategoryPreferences$9#this$009'),
        _MakeSym(TEXT, 44, 'a', name='.L.ref.tmp.137'),
    ]
    d = diff.Diff(size_info1, size_info2)
    self.assertEquals((0, 0, 0), d.raw_symbols.CountsByDiffStatus()[1:])
    self.assertEquals(0, d.raw_symbols.size)

  def testChangedParams(self):
    # Ensure that params changes match up so long as path doesn't change.
    size_info1 = _CreateSizeInfo()
    size_info1.raw_symbols[0].full_name = 'Foo()'
    size_info1.raw_symbols[0].name = 'Foo'
    size_info2 = _CreateSizeInfo()
    size_info2.raw_symbols[0].full_name = 'Foo(bool)'
    size_info2.raw_symbols[0].name = 'Foo'
    d = diff.Diff(size_info1, size_info2)
    self.assertEquals((0, 0, 0), d.raw_symbols.CountsByDiffStatus()[1:])
    self.assertEquals(0, d.raw_symbols.size)

  def testChangedPaths_Native(self):
    # Ensure that non-globally-unique symbols are not matched when path changes.
    size_info1 = _CreateSizeInfo()
    size_info2 = _CreateSizeInfo()
    size_info2.raw_symbols[1].object_path = 'asdf'
    d = diff.Diff(size_info1, size_info2)
    self.assertEquals((0, 1, 1), d.raw_symbols.CountsByDiffStatus()[1:])
    self.assertEquals(0, d.raw_symbols.size)

  def testChangedPaths_StringLiterals(self):
    # Ensure that string literals are not matched up.
    size_info1 = _CreateSizeInfo()
    size_info1.raw_symbols[0].full_name = models.STRING_LITERAL_NAME
    size_info2 = _CreateSizeInfo()
    size_info2.raw_symbols[0].full_name = models.STRING_LITERAL_NAME
    size_info2.raw_symbols[0].object_path = 'asdf'
    d = diff.Diff(size_info1, size_info2)
    self.assertEquals((0, 1, 1), d.raw_symbols.CountsByDiffStatus()[1:])
    self.assertEquals(0, d.raw_symbols.size)

  def testChangedPaths_Java(self):
    # Ensure that Java symbols are matched up.
    size_info1 = _CreateSizeInfo()
    size_info2 = _CreateSizeInfo()
    size_info2.raw_symbols[0].object_path = 'asdf'
    d = diff.Diff(size_info1, size_info2)
    self.assertEquals((0, 0, 0), d.raw_symbols.CountsByDiffStatus()[1:])
    self.assertEquals(0, d.raw_symbols.size)

  def testChangedPaths_ChangedParams(self):
    # Ensure that path changes are not matched when params also change.
    size_info1 = _CreateSizeInfo()
    size_info1.raw_symbols[0].full_name = 'Foo()'
    size_info1.raw_symbols[0].name = 'Foo'
    size_info2 = _CreateSizeInfo()
    size_info2.raw_symbols[0].full_name = 'Foo(bool)'
    size_info2.raw_symbols[0].name = 'Foo'
    size_info2.raw_symbols[0].object_path = 'asdf'
    d = diff.Diff(size_info1, size_info2)
    self.assertEquals((0, 1, 1), d.raw_symbols.CountsByDiffStatus()[1:])
    self.assertEquals(0, d.raw_symbols.size)



if __name__ == '__main__':
  unittest.main()
