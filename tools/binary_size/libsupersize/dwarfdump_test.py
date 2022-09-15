#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import dwarfdump


class DwarfDumpTest(unittest.TestCase):
  def _MakeRangeInfoList(self, flat_list):
    out = []
    for item in flat_list:
      assert len(item) == 3
      out.append((dwarfdump._AddressRange(item[0], item[1]), item[2]))
    return out

  def testParseNonContiguousAddressRange(self):
    """Test parsing DW_TAG_compile_unit with non-contiguous address range."""
    lines = [
        'DW_TAG_compile_unit',
        'DW_AT_name ("solution.cc")',
        'DW_AT_low_pc (0x0)',
        'DW_AT_ranges (0x1',
        '[0x10, 0x21)',
        '[0x31, 0x41))',
    ]
    expected_info_list = [(0x10, 0x21, 'solution.cc'),
                          (0x31, 0x41, 'solution.cc')]
    self.assertEqual(self._MakeRangeInfoList(expected_info_list),
                     dwarfdump.ParseDumpOutputForTest(lines))

  def testParseNonContiguousAddressRangeOtherBrackets(self):
    """Test parsing DW_AT_ranges when non-standard brackets are used."""
    lines = [
        'DW_TAG_compile_unit',
        'DW_AT_name ("solution.cc")',
        'DW_AT_low_pc (0x0)',
        'DW_AT_ranges [0x1',
        '(0x10, 0x21)',
        '[0x31, 0x41]]',
    ]
    expected_info_list = [(0x10, 0x21, 'solution.cc'),
                          (0x31, 0x41, 'solution.cc')]
    self.assertEqual(self._MakeRangeInfoList(expected_info_list),
                     dwarfdump.ParseDumpOutputForTest(lines))

  def testParseNonContiguousIgnoreEmptyRanges(self):
    """Test that empty ranges are ignored when parsing DW_AT_ranges."""
    lines = [
        'DW_TAG_compile_unit',
        'DW_AT_name ("solution.cc")',
        'DW_AT_ranges (0x1',
        '[0x1, 0x1)',
        '[0x10, 0x21)',
        '[0x22, 0x22)',
        '[0x31, 0x41)',
        '[0x42, 0x42))',
    ]
    expected_info_list = [(0x10, 0x21, 'solution.cc'),
                          (0x31, 0x41, 'solution.cc')]
    self.assertEqual(self._MakeRangeInfoList(expected_info_list),
                     dwarfdump.ParseDumpOutputForTest(lines))

  def testParseContiguousAddressRange(self):
    """Test parsing DW_TAG_compile_unit with contiguous address range."""
    lines = [
        'DW_TAG_compile_unit',
        'DW_AT_name ("solution.cc")',
        'DW_AT_low_pc (0x1)',
        'DW_AT_high_pc (0x10)',
    ]
    expected_info_list = [
        (0x1, 0x10, 'solution.cc'),
    ]
    self.assertEqual(self._MakeRangeInfoList(expected_info_list),
                     dwarfdump.ParseDumpOutputForTest(lines))

  def testParseSingleAddress(self):
    """Test parsing DW_TAG_compile_unit with single address."""
    lines = [
        'DW_TAG_compile_unit',
        'DW_AT_name ("solution.cc")',
        'DW_AT_low_pc (0x10)',
    ]
    expected_info_list = [
        (0x10, 0x11, 'solution.cc'),
    ]
    self.assertEqual(self._MakeRangeInfoList(expected_info_list),
                     dwarfdump.ParseDumpOutputForTest(lines))

  def testParseEmptyCompileUnit(self):
    """Test parsing empty DW_TAG_compile_unit."""
    lines = ['DW_TAG_compile_unit']
    self.assertEqual([], dwarfdump.ParseDumpOutputForTest(lines))

  def testConsecutiveCompileUnits(self):
    """Test parsing consecutive DW_TAG_compile_units."""
    lines = [
        'DW_TAG_compile_unit',
        'DW_AT_name ("foo.cc")',
        'DW_AT_low_pc (0x1)',
        'DW_AT_high_pc (0x10)',
        'DW_TAG_compile_unit',
        'DW_AT_name ("bar.cc")',
        'DW_AT_low_pc (0x12)',
        'DW_AT_high_pc (0x20)',
    ]
    expected_info_list = [(0x1, 0x10, 'foo.cc'), (0x12, 0x20, 'bar.cc')]
    self.assertEqual(self._MakeRangeInfoList(expected_info_list),
                     dwarfdump.ParseDumpOutputForTest(lines))

  def testTagTerminatedCompileUnit(self):
    """Test parsing DW_TAG_compile_unit where compile unit is followed by a
    non-DW_TAG_compile_unit entry.
    """
    lines = [
        'DW_TAG_compile_unit',
        'DW_AT_name ("foo.cc")',
        'DW_AT_low_pc (0x1)',
        'DW_AT_high_pc (0x10)',
        'DW_TAG_subprogram',
        'DW_AT_name ("bar.cc")',
        'DW_AT_low_pc (0x12)',
        'DW_AT_high_pc (0x20)',
    ]
    expected_info_list = [
        (0x1, 0x10, 'foo.cc'),
    ]
    self.assertEqual(self._MakeRangeInfoList(expected_info_list),
                     dwarfdump.ParseDumpOutputForTest(lines))

  def testHandlePrefixes(self):
    """Test parsing DW_TAG_compile_unit where 'DW_' does not start line in
    DW_TAG_compile_unit entry.
    """
    lines = [
        '0x1 DW_TAG_compile_unit',
        '    DW_AT_language (DW_LANG_C_plus_plus_14)',
        '    DW_AT_name     ("solution.cc")',
        '    DW_AT_stmt_list (0x5)',
        '    DW_AT_low_pc   (0x1)',
        '    DW_AT_high_pc  (0x10)',
    ]
    expected_info_list = [
        (0x1, 0x10, 'solution.cc'),
    ]
    self.assertEqual(self._MakeRangeInfoList(expected_info_list),
                     dwarfdump.ParseDumpOutputForTest(lines))

  def testFindAddress(self):
    """Tests for _SourceMapper.FindSourceForTextAddress()"""
    lines = [
        'DW_TAG_compile_unit',
        'DW_AT_name     ("foo.cc")',
        'DW_AT_low_pc   (0x1)',
        'DW_AT_high_pc  (0x10)',
        'DW_TAG_compile_unit',
        'DW_AT_name     ("bar.cc")',
        'DW_AT_low_pc   (0x21)',
        'DW_AT_high_pc  (0x30)',
        'DW_TAG_compile_unit',
        'DW_AT_name     ("baz.cc")',
        'DW_AT_low_pc   (0x41)',
        'DW_AT_high_pc  (0x50)',
    ]
    source_mapper = dwarfdump.CreateAddressSourceMapperForTest(lines)
    # Address is before first range.
    self.assertEqual('', source_mapper.FindSourceForTextAddress(0x0))
    # Address matches start of first range.
    self.assertEqual('foo.cc', source_mapper.FindSourceForTextAddress(0x1))
    # Address is in the middle of middle range.
    self.assertEqual('bar.cc', source_mapper.FindSourceForTextAddress(0x2a))
    # Address matches end of last range.
    self.assertEqual('baz.cc', source_mapper.FindSourceForTextAddress(0x4f))
    # Address is after lange range.
    self.assertEqual('', source_mapper.FindSourceForTextAddress(0x50))


if __name__ == '__main__':
  unittest.main()
