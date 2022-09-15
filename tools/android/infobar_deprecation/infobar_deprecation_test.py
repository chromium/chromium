#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest
import infobar_deprecation

sys.path.append(
    os.path.dirname(
        os.path.dirname(
            os.path.dirname(os.path.dirname(os.path.abspath(__file__))))))
from PRESUBMIT_test_mocks import MockFile, MockInputApi, MockOutputApi

_MOCK_INFOBAR_DELEGATE_H_CONTENTS = '''
// random comments
enum InfoBarIdentifier {
  A_INFOBAR_ANDROID = 1,
  B_INFOBAR_MOBILE = 2,
  // OBSOLETE_INFOBAR_ANDROID obsolete infobar,
  C_INFOBAR_IOS = 3,
  D_INFOBAR = 4,
  E_INFOBAR = 5,
  F_INFOBAR_ANDROID = 6,
  F_INFOBAR_MOBILE = 7,
  F_INFOBAR_IOS = 8,
  // G_INFOBAR_ANDROID = 9,
};
// random comments

'''


class InfobarDeprecationMockInputApi(MockInputApi):
  def ReadFile(self, filename, mode='rU'):
    return _MOCK_INFOBAR_DELEGATE_H_CONTENTS


class InfobarDeprecationMockInputFile(MockFile):
  def ChangedContents(self):
    return [
        (6, '  E_INFOBAR = 5,'),
        (7, '  F_INFOBAR_ANDROID = 6,'),
        (8, '  F_INFOBAR_MOBILE = 7,'),
        (9, '  F_INFOBAR_IOS = 8,'),
        (10, '  // G_INFOBAR_ANDROID = 9,'),
    ]


class TestInfobarDeprecation(unittest.TestCase):
  def testInfoBarIdentifier(self):
    lines = []

    mock_input_api = MockInputApi()
    mock_input_api.files = [
        MockFile(
            os.path.join(infobar_deprecation.INFOBAR_ANDROID_FOLDERS[0],
                         'TestInfobarDelegate.java'), lines),
        MockFile(
            os.path.join(infobar_deprecation.INFOBAR_ANDROID_FOLDERS[1],
                         'test_infobar_delegate.cc'), lines),

        # Add un-related file for testing.
        MockFile(
            os.path.join(infobar_deprecation.INFOBAR_ANDROID_FOLDERS[1],
                         'README.md'), lines),
    ]
    warnings = infobar_deprecation._CheckNewInfobar(mock_input_api,
                                                    MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertEqual(2, len(warnings[0].items))

  def testInfobarFiles(self):
    mock_input_api = InfobarDeprecationMockInputApi()
    mock_input_api.files = [
        InfobarDeprecationMockInputFile(infobar_deprecation.INFOBAR_DELEGATE_H,
                                        []),
    ]
    warnings = infobar_deprecation._CheckNewInfobar(mock_input_api,
                                                    MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertEqual(2, len(warnings[0].items))


if __name__ == '__main__':
  unittest.main()
