#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import ast
import itertools
import os
import unittest

import bcanalyzer
import parallel


_SCRIPT_DIR = os.path.dirname(__file__)
_TEST_DATA_DIR = os.path.join(_SCRIPT_DIR, 'testdata')
_TEST_SOURCE_DIR = os.path.join(_TEST_DATA_DIR, 'mock_source_directory')
_TEST_OUTPUT_DIR = os.path.join(_TEST_SOURCE_DIR, 'out', 'Release')
_TEST_TOOL_PREFIX = os.path.join(
    os.path.abspath(_TEST_DATA_DIR), 'mock_toolchain', 'llvm-')


def _MakeBytes(bits, toks):
  """Creates a multi-byte string from ASCII strings and/or ASCII codes.

  Args:
    bits: Number of bits per character, must be in {8, 16, 32}.
    toks: A list of tokens, each of which is a ASCII strings or an integer
      representing a character's ASCII value (e.g., 0 for terminating null).

  Returns: A flattened string of the |bits|-bit string formed by |bit|-extending
    the result of concatanating tokens.
  """
  cfi = itertools.chain.from_iterable
  chars = cfi(map(ord, t) if isinstance(t, str) else (t, ) for t in toks)
  return bytes(cfi((c >> (8 * i) for i in range(bits // 8)) for c in chars))


class BcAnalyzerTest(unittest.TestCase):

  def testParseTag(self):
    # Valid cases.
    self.assertEqual((bcanalyzer.OPENING_TAG, 'FOO', 4),
                     bcanalyzer.ParseTag('<FOO> trailing'))
    self.assertEqual((bcanalyzer.OPENING_TAG, 'BAR', 4),
                     bcanalyzer.ParseTag('<BAR op0=3 op1=4>'))
    self.assertEqual((bcanalyzer.CLOSING_TAG, 'FOO', 5),
                     bcanalyzer.ParseTag('</FOO>'))
    self.assertEqual((bcanalyzer.SELF_CLOSING_TAG, 'FOO', 4),
                     bcanalyzer.ParseTag('<FOO/>'))
    self.assertEqual((bcanalyzer.SELF_CLOSING_TAG, 'TOMATO2', 8),
                     bcanalyzer.ParseTag('<TOMATO2   />'))
    # Not self-closing: For simplicity we requires '/>' with space.
    self.assertEqual((bcanalyzer.OPENING_TAG, 'TOMATO2', 8),
                     bcanalyzer.ParseTag('<TOMATO2  / >'))
    self.assertEqual((bcanalyzer.SELF_CLOSING_TAG, 'BAR', 4),
                     bcanalyzer.ParseTag('<BAR op0=3 op1=4/>'))
    self.assertEqual((bcanalyzer.OPENING_TAG, 'FOO', 4),
                     bcanalyzer.ParseTag('<FOO> / trailing'))
    self.assertEqual((bcanalyzer.SELF_CLOSING_TAG, 'STRUCT_NAME', 12),
                     bcanalyzer.ParseTag('<STRUCT_NAME abbrevid=7 op0=0/>'))
    self.assertEqual((bcanalyzer.SELF_CLOSING_TAG, 'UnkownCode41', 13),
                     bcanalyzer.ParseTag('<UnkownCode41 op0=0 op1=4 op2=5/>'))
    self.assertEqual((bcanalyzer.CLOSING_TAG, 'FOO_BAR', 9),
                     bcanalyzer.ParseTag('</FOO_BAR> \'/>trailing\''))
    self.assertEqual((bcanalyzer.OPENING_TAG, 'A', 2),
                     bcanalyzer.ParseTag('<A>'))
    self.assertEqual((bcanalyzer.OPENING_TAG, 'lower', 6),
                     bcanalyzer.ParseTag('<lower >'))
    # An invalid tag (all numbers), but we allow for simplicity.
    self.assertEqual((bcanalyzer.OPENING_TAG, '123', 4),
                     bcanalyzer.ParseTag('<123>'))
    # Abominations that are allowed for simplicity.
    self.assertEqual((bcanalyzer.SELF_CLOSING_TAG, 'FOO', 5),
                     bcanalyzer.ParseTag('</FOO/>'))
    self.assertEqual((bcanalyzer.SELF_CLOSING_TAG, 'FOO', 4),
                     bcanalyzer.ParseTag('<FOO///////>'))
    self.assertEqual((bcanalyzer.OPENING_TAG, 'FOO', 4),
                     bcanalyzer.ParseTag('<FOO>>>>>'))

    # Invalid cases.
    None3 = (None, None, None)
    self.assertEqual(None3, bcanalyzer.ParseTag(''))
    self.assertEqual(None3, bcanalyzer.ParseTag('     '))
    self.assertEqual(None3, bcanalyzer.ParseTag('<>'))
    self.assertEqual(None3, bcanalyzer.ParseTag('<>      '))
    self.assertEqual(None3, bcanalyzer.ParseTag('< >'))
    self.assertEqual(None3, bcanalyzer.ParseTag('<<FOO>'))
    self.assertEqual(None3, bcanalyzer.ParseTag('< FOO>'))
    self.assertEqual(None3, bcanalyzer.ParseTag('<//FOO>'))
    self.assertEqual(None3, bcanalyzer.ParseTag('</ FOO>'))
    self.assertEqual(None3, bcanalyzer.ParseTag('< FOO />'))
    self.assertEqual(None3, bcanalyzer.ParseTag('<FOO<>'))
    self.assertEqual(None3, bcanalyzer.ParseTag('<NOEND'))
    self.assertEqual(None3, bcanalyzer.ParseTag('some text'))
    self.assertEqual(None3, bcanalyzer.ParseTag('    <UNTRIMMED>'))
    self.assertEqual(None3, bcanalyzer.ParseTag('&lt;AAA&gt;'))

  def testAnalyzer(self):
    # Save global param in bcanalyzer.
    saved_char_width_limit = bcanalyzer._CHAR_WIDTH_LIMIT

    for width_limit, include_4byte_strings in [(2, False), (4, True)]:
      # Tweak global param in bcanalyzer.
      bcanalyzer._CHAR_WIDTH_LIMIT = width_limit

      encoded_results = bcanalyzer.RunBcAnalyzerOnIntermediates(
          ['test.o'], _TEST_TOOL_PREFIX, _TEST_OUTPUT_DIR)
      results = parallel.DecodeDictOfLists(
          encoded_results, value_transform=ast.literal_eval)
      self.assertEqual(['test.o'], list(results.keys()))
      str_list = results['test.o']

      # See mock_bcanalyzer.py for details on the C++ test file.
      expected = []
      expected.append(_MakeBytes(8, ['Test1a', 0]))
      expected.append(_MakeBytes(8, ['Test1b', 0]))
      expected.append(_MakeBytes(8, ['Test2a', 0]))
      expected.append(_MakeBytes(8, ['Test2b', 0]))
      expected.append(_MakeBytes(16, ['Test3a', 0]))
      expected.append(_MakeBytes(16, ['Test3b', 0]))
      if include_4byte_strings:
        expected.append(_MakeBytes(32, ['Test4a', 0]))
        expected.append(_MakeBytes(32, ['Test4b', 0]))
      expected.append(_MakeBytes(8, [1, 0, 0, 1, 1, 0]))
      expected.append(_MakeBytes(8, [1, 0, 0, 1, 1, 1]))
      expected.append(_MakeBytes(8, ['Test5a', 0]))
      expected.append(_MakeBytes(8, ['Test5b', 1]))
      expected.append(_MakeBytes(16, ['Test6a', 0]))
      expected.append(_MakeBytes(16, ['Test6b', 1]))
      if include_4byte_strings:
        expected.append(_MakeBytes(32, ['Test7a', 0]))
        expected.append(_MakeBytes(32, ['Test7b', 1]))
      expected.append(_MakeBytes(8, ['Test8a', 0]))
      expected.append(_MakeBytes(8, ['Test8b', 0]))
      # Exclude |{u8a, u8b, u16a, u16b, u32a, u32b, u64a, u64b}|.
      # Exclude |{s8empty, s16empty, s32empty}|.
      expected.append(_MakeBytes(8, ['1a', 0]))
      # Exclude |zeros|, which should be in .bss section.

      self.assertEqual(expected, str_list)

    # Restore globa param in bcanalyzer.
    bcanalyzer._CHAR_WIDTH_LIMIT = saved_char_width_limit

if __name__ == '__main__':
  unittest.main()
