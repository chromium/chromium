#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

import r8_disassembly
import test_util


_TEST_DATA_DIR = test_util.TEST_DATA_DIR


class DexDisassemblyTest(unittest.TestCase):
  @classmethod
  def setUpClass(cls):
    with open(os.path.join(_TEST_DATA_DIR, 'R8_Disassembler_Output.txt'),
              'r') as f:
      cls.class_obj_map, _ = r8_disassembly.Parse(f)

  def testParseClassName(self):
    """Test parsing the class names."""
    expected_info_list = [
        'org.chromium.chrome.browser.customtabs.' +
        'CustomTabDelegateFactory$$Lambda$5',
        'org.chromium.chrome.browser.app.appmenu.IncognitoMenuItemViewBinder$1',
        'com.youtube.elements.fbs.AnimatedVectorType'
    ]
    self.assertEqual(expected_info_list, list(self.class_obj_map))

  def testParseMethodList(self):
    """Test parsing the method names for a class."""
    expected_info_list_class1 = ['<init>', 'get']
    expected_info_list_class2 = []
    expected_info_list_class3 = [
        '<init>', 'animation', 'frameState', 'progressState'
    ]
    self.assertEqual(expected_info_list_class1, [
        method.name for method in
        self.class_obj_map['org.chromium.chrome.browser.customtabs.' +
                           'CustomTabDelegateFactory$$Lambda$5'].methods
    ])
    self.assertEqual(expected_info_list_class2, [
        method.name for method in
        self.class_obj_map['org.chromium.chrome.browser.app.appmenu.' +
                           'IncognitoMenuItemViewBinder$1'].methods
    ])
    self.assertEqual(expected_info_list_class3, [
        method.name for method in self.
        class_obj_map['com.youtube.elements.fbs.AnimatedVectorType'].methods
    ])

  def testParseMethodReturnType(self):
    """Test parsing the return type for each method."""
    # Note the return types are obfuscated.
    expected_info_list_class1 = ['void', 'java.lang.Object']
    expected_info_list_class2 = []
    expected_info_list_class3 = ['void', 'bb', 'Va', 'Wa']
    self.assertEqual(expected_info_list_class1, [
        method.return_type for method in self.class_obj_map[
            'org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory' +
            '$$Lambda$5'].methods
    ])
    self.assertEqual(expected_info_list_class2, [
        method.return_type for method in
        self.class_obj_map['org.chromium.chrome.browser.app.appmenu' +
                           '.IncognitoMenuItemViewBinder$1'].methods
    ])
    self.assertEqual(expected_info_list_class3, [
        method.return_type for method in self.
        class_obj_map['com.youtube.elements.fbs.AnimatedVectorType'].methods
    ])

  def testParseMethodParamType(self):
    """Test parsing the parameters type for each method."""
    # Note the return types are obfuscated.
    expected_info_list_class1 = [[], []]
    expected_info_list_class2 = []
    expected_info_list_class3 = [[], [], ['java.lang.Object', 'int', 'byte[]'],
                                 ['java.lang.Object']]
    self.assertEqual(expected_info_list_class1, [
        method.param_types for method in self.class_obj_map[
            'org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory' +
            '$$Lambda$5'].methods
    ])
    self.assertEqual(expected_info_list_class2, [
        method.param_types for method in
        self.class_obj_map['org.chromium.chrome.browser.app.appmenu.' +
                           'IncognitoMenuItemViewBinder$1'].methods
    ])
    self.assertEqual(expected_info_list_class3, [
        method.param_types for method in self.
        class_obj_map['com.youtube.elements.fbs.AnimatedVectorType'].methods
    ])

  def testParseMethodBytecode(self):
    """Test parsing a stand alone class."""
    # Note the return types are obfuscated.
    expected_info = [
        'registers: 1, inputs: 1, outputs: 1\n',
        '------------------------------------------------------------\n',
        'inst#  offset  instruction         arguments\n',
        '------------------------------------------------------------\n',
        '    0:   0x00: InvokeDirect        { v0 } org.chromium.base.' +
        'supplier.Supplier$$CC void <init>()\n', '    1:   0x03: ReturnVoid\n'
    ]
    self.assertEqual(
        expected_info, self.class_obj_map[
            'org.chromium.chrome.browser.customtabs.' +
            'CustomTabDelegateFactory$$Lambda$5'].FindMethodByteCode(
                'org.chromium.chrome.browser.customtabs.' +
                'CustomTabDelegateFactory$$Lambda$5', '<init>', [], 'void'))


if __name__ == '__main__':
  unittest.main()
