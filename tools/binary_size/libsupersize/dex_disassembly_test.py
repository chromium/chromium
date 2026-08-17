#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import os
import unittest
import unittest.mock as mock

import r8_disassembly
import test_util
import dex_disassembly


_TEST_DATA_DIR = test_util.TEST_DATA_DIR


class DexDisassemblyTest(unittest.TestCase):
  @classmethod
  def setUpClass(cls):
    with open(
      os.path.join(_TEST_DATA_DIR, 'R8_Disassembler_Output.txt'), 'r'
    ) as f:
      cls.class_obj_map, _ = r8_disassembly.Parse(f)

  def testParseClassName(self):
    """Test parsing the class names."""
    expected_info_list = [
      'org.chromium.chrome.browser.customtabs.'
      + 'CustomTabDelegateFactory$$Lambda$5',
      'org.chromium.chrome.browser.app.appmenu.IncognitoMenuItemViewBinder$1',
      'com.youtube.elements.fbs.AnimatedVectorType',
    ]
    self.assertEqual(expected_info_list, list(self.class_obj_map))

  def testParseMethodList(self):
    """Test parsing the method names for a class."""
    expected_info_list_class1 = ['<init>', 'get']
    expected_info_list_class2 = []
    expected_info_list_class3 = [
      '<init>',
      'animation',
      'frameState',
      'progressState',
    ]
    self.assertEqual(
      expected_info_list_class1,
      [
        method.name
        for method in self.class_obj_map[
          'org.chromium.chrome.browser.customtabs.'
          + 'CustomTabDelegateFactory$$Lambda$5'
        ].methods
      ],
    )
    self.assertEqual(
      expected_info_list_class2,
      [
        method.name
        for method in self.class_obj_map[
          'org.chromium.chrome.browser.app.appmenu.'
          + 'IncognitoMenuItemViewBinder$1'
        ].methods
      ],
    )
    self.assertEqual(
      expected_info_list_class3,
      [
        method.name
        for method in self.class_obj_map[
          'com.youtube.elements.fbs.AnimatedVectorType'
        ].methods
      ],
    )

  def testParseMethodReturnType(self):
    """Test parsing the return type for each method."""
    # Note the return types are obfuscated.
    expected_info_list_class1 = ['void', 'java.lang.Object']
    expected_info_list_class2 = []
    expected_info_list_class3 = ['void', 'bb', 'Va', 'Wa']
    self.assertEqual(
      expected_info_list_class1,
      [
        method.return_type
        for method in self.class_obj_map[
          'org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory'
          + '$$Lambda$5'
        ].methods
      ],
    )
    self.assertEqual(
      expected_info_list_class2,
      [
        method.return_type
        for method in self.class_obj_map[
          'org.chromium.chrome.browser.app.appmenu'
          + '.IncognitoMenuItemViewBinder$1'
        ].methods
      ],
    )
    self.assertEqual(
      expected_info_list_class3,
      [
        method.return_type
        for method in self.class_obj_map[
          'com.youtube.elements.fbs.AnimatedVectorType'
        ].methods
      ],
    )

  def testParseMethodParamType(self):
    """Test parsing the parameters type for each method."""
    # Note the return types are obfuscated.
    expected_info_list_class1 = [[], []]
    expected_info_list_class2 = []
    expected_info_list_class3 = [
      [],
      [],
      ['java.lang.Object', 'int', 'byte[]'],
      ['java.lang.Object'],
    ]
    self.assertEqual(
      expected_info_list_class1,
      [
        method.param_types
        for method in self.class_obj_map[
          'org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory'
          + '$$Lambda$5'
        ].methods
      ],
    )
    self.assertEqual(
      expected_info_list_class2,
      [
        method.param_types
        for method in self.class_obj_map[
          'org.chromium.chrome.browser.app.appmenu.'
          + 'IncognitoMenuItemViewBinder$1'
        ].methods
      ],
    )
    self.assertEqual(
      expected_info_list_class3,
      [
        method.param_types
        for method in self.class_obj_map[
          'com.youtube.elements.fbs.AnimatedVectorType'
        ].methods
      ],
    )

  def testParseMethodBytecode(self):
    """Test parsing a stand alone class."""
    # Note the return types are obfuscated.
    expected_info = [
      'registers: 1, inputs: 1, outputs: 1\n',
      '------------------------------------------------------------\n',
      'inst#  offset  instruction         arguments\n',
      '------------------------------------------------------------\n',
      '    0:   0x00: InvokeDirect        { v0 } org.chromium.base.'
      + 'supplier.Supplier$$CC void <init>()\n',
      '    1:   0x03: ReturnVoid\n',
    ]
    self.assertEqual(
      expected_info,
      self.class_obj_map[
        'org.chromium.chrome.browser.customtabs.'
        + 'CustomTabDelegateFactory$$Lambda$5'
      ].FindMethodByteCode(
        'org.chromium.chrome.browser.customtabs.'
        + 'CustomTabDelegateFactory$$Lambda$5',
        '<init>',
        [],
        'void',
      ),
    )

  @mock.patch('dex_disassembly._CachedApkDisassembler')
  def testAddDisassemblyNormalized(self, mock_disassembler_cls):
    class_obj_map_after = copy.deepcopy(self.class_obj_map)
    class_obj_map_before = copy.deepcopy(self.class_obj_map)

    method_after = class_obj_map_after[
      'org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory'
      '$$Lambda$5'
    ].methods[0]
    method_before = class_obj_map_before[
      'org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory'
      '$$Lambda$5'
    ].methods[0]

    method_after.bytecode = [
      'registers: 1, inputs: 1, outputs: 1\n',
      '------------------------------------------------------------\n',
      'inst#  offset  instruction         arguments\n',
      '------------------------------------------------------------\n',
      '    0:   0x00: Goto                0x03 (+3)\n',
      '    1:   0x03: ReturnVoid          -> 0x03\n',
      '      [0x00 .. 0x03[\n',
    ]

    method_before.bytecode = [
      'registers: 1, inputs: 1, outputs: 1\n',
      '------------------------------------------------------------\n',
      'inst#  offset  instruction         arguments\n',
      '------------------------------------------------------------\n',
      '    0:   0x02: Goto                0x05 (+3)\n',
      '    1:   0x05: ReturnVoid          -> 0x05\n',
      '      [0x02 .. 0x05[\n',
    ]

    mock_after_disassembler = mock.MagicMock()
    mock_after_disassembler.GetForApkAndSplit.return_value = class_obj_map_after

    mock_before_disassembler = mock.MagicMock()
    mock_before_disassembler.GetForApkAndSplit.return_value = (
      class_obj_map_before
    )

    mock_disassembler_cls.side_effect = [
      mock_after_disassembler,
      mock_before_disassembler,
    ]

    mock_after_symbol = mock.MagicMock()
    mock_after_symbol.full_name = (
      'org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory'
      '$$Lambda$5#<init>(): void'
    )
    mock_after_symbol.container.metadata = {
      'proguard_mapping_file_name': 'mapping.txt',
      'apk_file_name': 'app.apk',
    }

    mock_before_symbol = mock.MagicMock()
    mock_before_symbol.full_name = mock_after_symbol.full_name
    mock_before_symbol.container = mock_after_symbol.container

    mock_delta_symbol = mock.MagicMock()
    mock_delta_symbol.after_symbol = mock_after_symbol
    mock_delta_symbol.before_symbol = mock_before_symbol
    mock_delta_symbol.section_name = 'dex.method'
    mock_delta_symbol.pss = 100
    mock_delta_symbol.full_name = mock_after_symbol.full_name

    mock_delta_size_info = mock.MagicMock()
    mock_sorted_symbols = mock.MagicMock()
    mock_group = mock.MagicMock()
    mock_group.name = mock_delta_symbol.diff_status
    mock_group.__iter__.return_value = [mock_delta_symbol]
    mock_sorted_symbols.GroupedByDiffStatus.return_value = [mock_group]
    mock_delta_size_info.raw_symbols.Filter.return_value = mock_sorted_symbols

    def dummy_path_resolver(path):
      return path

    # Call AddDisassembly with normalize=True
    dex_disassembly.AddDisassembly(
      mock_delta_size_info,
      dummy_path_resolver,
      dummy_path_resolver,
      normalize=True,
    )

    # Since they are normalized, they should be identical.
    self.assertEqual('', mock_after_symbol.disassembly)

    # Call AddDisassembly with normalize=False (default)
    mock_disassembler_cls.side_effect = [
      mock_after_disassembler,
      mock_before_disassembler,
    ]
    mock_after_symbol.disassembly = None

    dex_disassembly.AddDisassembly(
      mock_delta_size_info,
      dummy_path_resolver,
      dummy_path_resolver,
      normalize=False,
    )

    # Without normalization, they should differ, so diff should NOT be empty.
    self.assertNotEqual('', mock_after_symbol.disassembly)
    self.assertIn(
      '-    0:   0x02: Goto                0x05 (+3)',
      mock_after_symbol.disassembly,
    )
    self.assertIn(
      '+    0:   0x00: Goto                0x03 (+3)',
      mock_after_symbol.disassembly,
    )
    self.assertIn('-      [0x02 .. 0x05[', mock_after_symbol.disassembly)
    self.assertIn('+      [0x00 .. 0x03[', mock_after_symbol.disassembly)

  def testNormalizeLines(self):
    lines = [
      '    0:   0x00: Goto                0x03 (+3)\n',
      '    1:   0x03: ReturnVoid          -> 0x03\n',
      '      [0x00 .. 0x03[\n',
    ]
    expected = [
      '<target>:\n',
      'Goto                <target>\n',
      '<target>:\n',
      'ReturnVoid          -> <target>\n',
      '      [<target> .. <target>[\n',
    ]
    actual = dex_disassembly.NormalizeLines(lines)
    self.assertEqual(expected, actual)


if __name__ == '__main__':
  unittest.main()
