#!/usr/bin/env vpython
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import symbol_extractor


# The number of spaces that objdump prefixes each symbol with.
SPACES = ' ' * 14


class TestLlvmBitcodeSymbolExtractor(unittest.TestCase):

  def testBasicParsing(self):
    data = '''-------- T _ZN4base11GetFileSizeERKNS_8FilePathEPx
aaaaaaaa W _ZNKSt3__19basic_iosIcNS_11char_traitsIcEEE5widenEc
-------- T _ZN4base13ContentsEqualERKNS_8FilePathES2_
---------------- T _SymbolWithA64BitAddress_
00000000 W _ZNSt3__113basic_filebufIcNS_11char_traitsIcEEE11__read_modeEv'''
    lines = data.split('\n')

    symbol_names = symbol_extractor._SymbolInfosFromLlvmNm(lines)
    self.assertEqual(5, len(symbol_names))
    self.assertListEqual(
        ['_ZN4base11GetFileSizeERKNS_8FilePathEPx',
         '_ZNKSt3__19basic_iosIcNS_11char_traitsIcEEE5widenEc',
         '_ZN4base13ContentsEqualERKNS_8FilePathES2_',
         '_SymbolWithA64BitAddress_',
         '_ZNSt3__113basic_filebufIcNS_11char_traitsIcEEE11__read_modeEv'],
        symbol_names)


class TestSymbolInfo(unittest.TestCase):

  def testIgnoresBlankLine(self):
    symbol_info = symbol_extractor._FromObjdumpLine('')
    self.assertIsNone(symbol_info)

  def testIgnoresMalformedLine(self):
    # This line is too short: only 6 flags.
    line = ('00c1b228      F .text\t00000060' + SPACES + '_ZN20trace_event')
    symbol_info = symbol_extractor._FromObjdumpLine(line)
    self.assertIsNone(symbol_info)

  def testWrongSymbolType(self):
    # This line has unsupported 'f' as symbol type.
    line = '00c1b228 l     f .text\t00000060' + SPACES + '_ZN20trace_event'
    self.assertRaises(AssertionError, symbol_extractor._FromObjdumpLine, line)

  def testAssertionErrorOnInvalidLines(self):
    # This line has an invalid scope.
    line = ('00c1b228 z     F .text\t00000060' + SPACES + '_ZN20trace_event')
    self.assertRaises(AssertionError, symbol_extractor._FromObjdumpLine, line)
    # This line has the symbol name with spaces in it.
    line = ('00c1b228 l     F .text\t00000060' + SPACES +
            '_ZN20trace_event too many')
    self.assertRaises(AssertionError, symbol_extractor._FromObjdumpLine, line)
    # This line has invalid characters in the symbol name.
    line = ('00c1b228 l     F .text\t00000060' + SPACES + '_ZN20trace_?bad')
    self.assertRaises(AssertionError, symbol_extractor._FromObjdumpLine, line)
    # This line has an invalid character at the start of the symbol name.
    line = ('00c1b228 l     F .text\t00000060' + SPACES + '$_ZN20trace_bad')
    self.assertRaises(AssertionError, symbol_extractor._FromObjdumpLine, line)

  def testSymbolTypeObject(self):
    # Builds with ThinLTO produce symbols of type 'O'.
    line = ('009faf60 l     O .text\t00000500' + SPACES + 'AES_Td')
    symbol_info = symbol_extractor._FromObjdumpLine(line)
    self.assertIsNotNone(symbol_info)
    self.assertEquals(0x009faf60, symbol_info.offset)
    self.assertEquals('.text', symbol_info.section)
    self.assertEquals(0x500, symbol_info.size)
    self.assertEquals('AES_Td', symbol_info.name)

  def testSymbolFromLocalLabel(self):
    line = ('00f64b80 l       .text\t00000000' + SPACES + 'Builtins_Abort')
    symbol_info = symbol_extractor._FromObjdumpLine(line)
    self.assertIsNone(symbol_info)

  def testStartOfText(self):
    line = ('00918000 l       .text\t00000000' + SPACES +
            '.hidden linker_script_start_of_text')
    symbol_info = symbol_extractor._FromObjdumpLine(line)
    self.assertIsNotNone(symbol_info)
    self.assertEquals(0x00918000, symbol_info.offset)
    self.assertEquals('linker_script_start_of_text', symbol_info.name)

  def testSymbolInfo(self):
    line = ('00c1c05c l     F .text\t0000002c' + SPACES +
            '_GLOBAL__sub_I_chrome_main_delegate.cc')
    test_name = '_GLOBAL__sub_I_chrome_main_delegate.cc'
    test_offset = 0x00c1c05c
    test_size = 0x2c
    test_section = '.text'
    symbol_info = symbol_extractor._FromObjdumpLine(line)
    self.assertIsNotNone(symbol_info)
    self.assertEquals(test_offset, symbol_info.offset)
    self.assertEquals(test_size, symbol_info.size)
    self.assertEquals(test_name, symbol_info.name)
    self.assertEquals(test_section, symbol_info.section)

  def testHiddenSymbol(self):
    line = ('00c1c05c l     F .text\t0000002c' + SPACES +
            '.hidden _GLOBAL__sub_I_chrome_main_delegate.cc')
    test_name = '_GLOBAL__sub_I_chrome_main_delegate.cc'
    test_offset = 0x00c1c05c
    test_size = 0x2c
    test_section = '.text'
    symbol_info = symbol_extractor._FromObjdumpLine(line)
    self.assertIsNotNone(symbol_info)
    self.assertEquals(test_offset, symbol_info.offset)
    self.assertEquals(test_size, symbol_info.size)
    self.assertEquals(test_name, symbol_info.name)
    self.assertEquals(test_section, symbol_info.section)

  def testDollarInSymbolName(self):
    # A $ character elsewhere in the symbol name is fine.
    # This is an example of a lambda function name from Clang.
    line = ('00c1b228 l     F .text\t00000060' + SPACES +
            '_ZZL11get_globalsvENK3$_1clEv')
    symbol_info = symbol_extractor._FromObjdumpLine(line)
    self.assertIsNotNone(symbol_info)
    self.assertEquals(0xc1b228, symbol_info.offset)
    self.assertEquals(0x60, symbol_info.size)
    self.assertEquals('_ZZL11get_globalsvENK3$_1clEv', symbol_info.name)
    self.assertEquals('.text', symbol_info.section)

  def testOutlinedFunction(self):
    # Test that an outlined function is reported normally. Also note that
    # outlined functions are in 64 bit builds which have longer addresses.
    line = ('00000000020fab4c l     F .text\t0000000000000014' + SPACES +
            'OUTLINED_FUNCTION_4')
    symbol_info = symbol_extractor._FromObjdumpLine(line)
    self.assertIsNotNone(symbol_info)
    self.assertEquals(0x20fab4c, symbol_info.offset)
    self.assertEquals(0x14, symbol_info.size)
    self.assertEquals('OUTLINED_FUNCTION_4', symbol_info.name)
    self.assertEquals('.text', symbol_info.section)

  def testNeitherLocalNorGlobalSymbol(self):
    # This happens, see crbug.com/992884.
    # Symbol which is neither local nor global.
    line = '0287ae50  w    F .text\t000001e8              log2l'
    symbol_info = symbol_extractor._FromObjdumpLine(line)
    self.assertIsNotNone(symbol_info)
    self.assertEquals(0x287ae50, symbol_info.offset)
    self.assertEquals(0x1e8, symbol_info.size)
    self.assertEquals('log2l', symbol_info.name)
    self.assertEquals('.text', symbol_info.section)

class TestSymbolInfosFromStream(unittest.TestCase):

  def testSymbolInfosFromStream(self):
    lines = ['Garbage',
             '',
             '00c1c05c l     F .text\t0000002c' + SPACES + 'first',
             ''
             'more garbage',
             '00155 g     F .text\t00000012' + SPACES + 'second']
    symbol_infos = symbol_extractor._SymbolInfosFromStream(lines)
    self.assertEquals(len(symbol_infos), 2)
    first = symbol_extractor.SymbolInfo('first', 0x00c1c05c, 0x2c, '.text')
    self.assertEquals(first, symbol_infos[0])
    second = symbol_extractor.SymbolInfo('second', 0x00155, 0x12, '.text')
    self.assertEquals(second, symbol_infos[1])


class TestSymbolInfoMappings(unittest.TestCase):

  def setUp(self):
    self.symbol_infos = [
        symbol_extractor.SymbolInfo('firstNameAtOffset', 0x42, 42, '.text'),
        symbol_extractor.SymbolInfo('secondNameAtOffset', 0x42, 42, '.text'),
        symbol_extractor.SymbolInfo('thirdSymbol', 0x64, 20, '.text')]

  def testGroupSymbolInfosByOffset(self):
    offset_to_symbol_info = symbol_extractor.GroupSymbolInfosByOffset(
        self.symbol_infos)
    self.assertEquals(len(offset_to_symbol_info), 2)
    self.assertIn(0x42, offset_to_symbol_info)
    self.assertEquals(offset_to_symbol_info[0x42][0], self.symbol_infos[0])
    self.assertEquals(offset_to_symbol_info[0x42][1], self.symbol_infos[1])
    self.assertIn(0x64, offset_to_symbol_info)
    self.assertEquals(offset_to_symbol_info[0x64][0], self.symbol_infos[2])

  def testCreateNameToSymbolInfo(self):
    name_to_symbol_info = symbol_extractor.CreateNameToSymbolInfo(
        self.symbol_infos)
    self.assertEquals(len(name_to_symbol_info), 3)
    for i in range(3):
      name = self.symbol_infos[i].name
      self.assertIn(name, name_to_symbol_info)
      self.assertEquals(self.symbol_infos[i], name_to_symbol_info[name])

  def testSymbolCollisions(self):
    symbol_infos_with_collision = list(self.symbol_infos)
    symbol_infos_with_collision.append(symbol_extractor.SymbolInfo(
        'secondNameAtOffset', 0x84, 42, '.text'))

    # The symbol added above should not affect the output.
    name_to_symbol_info = symbol_extractor.CreateNameToSymbolInfo(
        self.symbol_infos)
    self.assertEquals(len(name_to_symbol_info), 3)
    for i in range(3):
      name = self.symbol_infos[i].name
      self.assertIn(name, name_to_symbol_info)
      self.assertEquals(self.symbol_infos[i], name_to_symbol_info[name])

if __name__ == '__main__':
  unittest.main()
