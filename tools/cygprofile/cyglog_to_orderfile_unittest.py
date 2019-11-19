#!/usr/bin/env vpython
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import collections
import os
import re
import sys
import unittest

import cyglog_to_orderfile
import process_profiles
import symbol_extractor

import test_utils
from test_utils import SimpleTestSymbol


sys.path.insert(
    0, os.path.join(os.path.dirname(__file__), os.pardir, os.pardir,
                    'build', 'android'))

import pylib.constants.host_paths as host_paths


# Used for fake demangling on bots where c++filt does not exist.
CTOR_PATTERN = re.compile(r'EEEC[12]Ev$')
CTOR_REPLACEMENT = 'EEECEv'
DTOR_PATTERN = re.compile(r'EEED[12]Ev$')
DTOR_REPLACEMENT = 'EEEDEv'


SectionTestSymbol = collections.namedtuple(
    'SectionTestSymbol', ['name', 'section'])


class TestObjectFileProcessor(cyglog_to_orderfile.ObjectFileProcessor):
  def __init__(self, symbol_to_sections):
    super(TestObjectFileProcessor, self).__init__(None)
    self._symbol_to_sections_map = symbol_to_sections


class TestCyglogToOrderfile(unittest.TestCase):
  def setUp(self):
    self._old_demangle = None
    if not os.path.exists(host_paths.ToolPath('c++filt', 'arm')):
      print('Using fake demangling due to missing c++filt binary')
      self._old_demangle = symbol_extractor.DemangleSymbol
      symbol_extractor.DemangleSymbol = _FakeDemangle

  def tearDown(self):
    if self._old_demangle is not None:
      symbol_extractor.DemangleSymbol = self._old_demangle

  def assertDictWithUnorderedListEqual(self, expected, observed):
    unexpected = set()
    missing = set()
    for i in expected:
      if i not in observed:
        missing.add(i)
      else:
        try:
          self.assertListEqual(sorted(expected[i]), sorted(observed[i]))
        except self.failureException, e:
          raise self.failureException('For key {}: {}'.format(i, e))
    for i in observed:
      # All i that are in expected have already been tested.
      if i not in expected:
        unexpected.add(i)
    failure_items = []
    if unexpected:
      failure_items.append('Unexpected keys: {}'.format(' '.join(unexpected)))
    if missing:
      failure_items.append('Missing keys: {}'.format(' '.join(missing)))
    if failure_items:
      raise self.failureException('\n'.join(failure_items))

  def testWarnAboutDuplicates(self):
    offsets = [0x1, 0x2, 0x3]
    self.assertTrue(cyglog_to_orderfile._WarnAboutDuplicates(offsets))
    offsets.append(0x1)
    self.assertFalse(cyglog_to_orderfile._WarnAboutDuplicates(offsets))

  def testSymbolsAtOffsetExactMatch(self):
    symbol_infos = [SimpleTestSymbol('1', 0x10, 0x13)]
    generator = cyglog_to_orderfile.OffsetOrderfileGenerator(
        test_utils.TestSymbolOffsetProcessor(symbol_infos), None)
    syms = generator._SymbolsAtOffset(0x10)
    self.assertEquals(1, len(syms))
    self.assertEquals(symbol_infos[0], syms[0])

  def testSymbolsAtOffsetInexectMatch(self):
    symbol_infos = [SimpleTestSymbol('1', 0x10, 0x13)]
    generator = cyglog_to_orderfile.OffsetOrderfileGenerator(
        test_utils.TestSymbolOffsetProcessor(symbol_infos), None)
    syms = generator._SymbolsAtOffset(0x11)
    self.assertEquals(1, len(syms))
    self.assertEquals(symbol_infos[0], syms[0])

  def testSameCtorOrDtorNames(self):
    same_name = cyglog_to_orderfile.ObjectFileProcessor._SameCtorOrDtorNames
    self.assertTrue(same_name(
        '_ZNSt3__119istreambuf_iteratorIcNS_11char_traitsIcEEEC1Ev',
        '_ZNSt3__119istreambuf_iteratorIcNS_11char_traitsIcEEEC2Ev'))
    self.assertTrue(same_name(
        '_ZNSt3__119istreambuf_iteratorIcNS_11char_traitsIcEEED1Ev',
        '_ZNSt3__119istreambuf_iteratorIcNS_11char_traitsIcEEED2Ev'))
    self.assertFalse(same_name(
        '_ZNSt3__119istreambuf_iteratorIcNS_11char_traitsIcEEEC1Ev',
        '_ZNSt3__119foo_iteratorIcNS_11char_traitsIcEEEC1Ev'))
    self.assertFalse(same_name(
        '_ZNSt3__119istreambuf_iteratorIcNS_11char_traitsIcEEE',
        '_ZNSt3__119istreambuf_iteratorIcNS_11char_traitsIcEEE'))

  def testGetSymbolToSectionsMap(self):
    processor = cyglog_to_orderfile.ObjectFileProcessor(None)
    processor._GetAllSymbolInfos = lambda: [
        SectionTestSymbol('.LTHUNK.foobar', 'unused'),
        SectionTestSymbol('_Znwj', '.text._Znwj'),
        SectionTestSymbol(  # Ctor/Dtor same name.
            '_ZNSt3__119istreambuf_iteratorIcNS_11char_traitsIcEEED1Ev',
            '.text._ZNSt3__119istreambuf_iteratorIcNS_11char_traitsIcEEED1Ev'),
        SectionTestSymbol(
            '_ZNSt3__119istreambuf_iteratorIcNS_11char_traitsIcEEED1Ev',
            '.text._ZNSt3__119istreambuf_iteratorIcNS_11char_traitsIcEEED2Ev'),
        SectionTestSymbol( # Ctor/Dtor different name.
            '_ZNSt3__119istreambuf2_iteratorIcNS_11char_traitsIcEEEC1Ev',
            ('.text. _ZNSt3__119istreambuf2_iteratorIcNS_11char_'
             'traitsIcEEEC1Ev')),
        SectionTestSymbol(
            '_ZNSt3__119istreambuf2_iteratorIcNS_11char_traitsIcEEEC1Ev',
            '.text._ZNSt3__119foo_iteratorIcNS_11char_traitsIcEEEC1Ev'),
        SectionTestSymbol('_AssemblyFunction', '.text'),
        SectionTestSymbol('_UnknownSection', '.surprise._UnknownSection'),
        SectionTestSymbol('_Znwj', '.text._another_section_for_Znwj')]
    self.assertDictWithUnorderedListEqual(
        {'_Znwj': ['.text._Znwj', '.text._another_section_for_Znwj'],
         # Ctor/Dtor same name.
         '_ZNSt3__119istreambuf_iteratorIcNS_11char_traitsIcEEED1Ev':
         ['.text._ZNSt3__119istreambuf_iteratorIcNS_11char_traitsIcEEED1Ev',
          '.text._ZNSt3__119istreambuf_iteratorIcNS_11char_traitsIcEEED2Ev'],
         # Ctor/Dtor different name; a warning is emitted but the sections are
         # still added to the map.
         '_ZNSt3__119istreambuf2_iteratorIcNS_11char_traitsIcEEEC1Ev':
         ['.text. _ZNSt3__119istreambuf2_iteratorIcNS_11char_traitsIcEEEC1Ev',
          '.text._ZNSt3__119foo_iteratorIcNS_11char_traitsIcEEEC1Ev'],
        },
        processor.GetSymbolToSectionsMap())

  def testOutputOrderfile(self):
    # One symbol not matched, one with an odd address, one regularly matched
    # And two symbols aliased to the same address
    symbols = [SimpleTestSymbol('Symbol', 0x10, 0x100),
               SimpleTestSymbol('Symbol2', 0x12, 0x100),
               SimpleTestSymbol('Symbol3', 0x16, 0x100),
               SimpleTestSymbol('Symbol3.2', 0x16, 0x0)]
    generator = cyglog_to_orderfile.OffsetOrderfileGenerator(
        test_utils.TestSymbolOffsetProcessor(symbols),
        TestObjectFileProcessor({
        'Symbol': ['.text.Symbol'],
        'Symbol2': ['.text.Symbol2', '.text.hot.Symbol2'],
        'Symbol3': ['.text.Symbol3'],
        'Symbol3.2': ['.text.Symbol3.2']}))
    ordered_sections = generator.GetOrderedSections([0x12, 0x17])
    self.assertListEqual(
        ['.text.Symbol2',
         '.text.hot.Symbol2',
         '.text.Symbol3',
         '.text.Symbol3.2'],
        ordered_sections)


def _FakeDemangle(mangled):
  unmangled = CTOR_PATTERN.sub(CTOR_REPLACEMENT, mangled)
  unmangled = DTOR_PATTERN.sub(DTOR_REPLACEMENT, unmangled)
  return unmangled


if __name__ == '__main__':
  unittest.main()
