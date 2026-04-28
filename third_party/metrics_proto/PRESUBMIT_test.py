#!/usr/bin/env python
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import PRESUBMIT

sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))

from PRESUBMIT_test_mocks import (MockInputApi, MockOutputApi, MockAffectedFile)


def FullPath(path):
  return os.path.join(MockInputApi().PresubmitLocalPath(), path)


class MetricsProtoCheckerTest(unittest.TestCase):

  def testModifiedWithoutReadme(self):
    input_api = MockInputApi()
    input_api.files = [MockAffectedFile(FullPath('somefile.proto'), 'some diff')]
    self.assertEqual(1, len(PRESUBMIT.CheckChange(input_api, MockOutputApi())))

  def testModifiedWithReadme(self):
    input_api = MockInputApi()
    input_api.files = [
      MockAffectedFile(FullPath('somefile.proto'), 'some diff'),
      MockAffectedFile(FullPath(PRESUBMIT.README), 'some diff'),
    ]
    self.assertEqual(0, len(PRESUBMIT.CheckChange(input_api, MockOutputApi())))

  def testModifiedReadmeOnly(self):
    input_api = MockInputApi()
    input_api.files = [
      MockAffectedFile(FullPath(PRESUBMIT.README), 'some diff'),
    ]
    self.assertEqual(0, len(PRESUBMIT.CheckChange(input_api, MockOutputApi())))

  def testExcludedPaths(self):
    input_api = MockInputApi()
    input_api.files = [
      # Changes to these files don't require README changes.
      MockAffectedFile(FullPath('PRESUBMIT.py'), 'some diff'),
      MockAffectedFile(FullPath('PRESUBMIT_test.py'), 'some diff'),
      MockAffectedFile(FullPath('OWNERS'), 'some diff'),
      MockAffectedFile(FullPath('DIR_METADATA'), 'some diff'),
    ]
    self.assertEqual(0, len(PRESUBMIT.CheckChange(input_api, MockOutputApi())))

  def testIncludedAndExcludedPaths(self):
    input_api = MockInputApi()
    input_api.files = [
      # Changes to these files don't require README changes.
      MockAffectedFile(FullPath('PRESUBMIT.py'), 'some diff'),
      MockAffectedFile(FullPath('PRESUBMIT_test.py'), 'some diff'),
      MockAffectedFile(FullPath('OWNERS'), 'some diff'),
      # But this one does.
      MockAffectedFile(FullPath('somefile.proto'), 'some diff'),
    ]
    self.assertEqual(1, len(PRESUBMIT.CheckChange(input_api, MockOutputApi())))

  def testPathOutsideLocalDir(self):
    input_api = MockInputApi()
    input_api.files = [
      MockAffectedFile('file_in_root_dir.proto', 'some diff'),
    ]
    self.assertEqual(0, len(PRESUBMIT.CheckChange(input_api, MockOutputApi())))


if __name__ == '__main__':
    unittest.main()
