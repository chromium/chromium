#!/usr/bin/env vpython3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Tests for compile_db."""

import sys
import unittest

import compile_db


# Input compile DB.
_TEST_COMPILE_DB = [
    # Verifies that rewrapper.exe is removed.
    {
        'command': r'C:\rewrapper.exe C:\clang-cl.exe /blah',
    },
    # Verifies a rewrapper path containing a space.
    {
        'command': r'"C:\Program Files\rewrapper.exe" C:\clang-cl.exe /blah',
    },
    # Includes a string define.
    {
        'command': r'clang-cl.exe /blah "-DCR_CLANG_REVISION=\"346388-1\""',
    },
    # Includes a string define with a space in it.
    {
        'command': r'clang-cl.exe /blah -D"MY_DEFINE=\"MY VALUE\""',
    },
]

# Expected compile DB after processing for windows.
_EXPECTED_COMPILE_DB = [
    {
        'command': r'C:\clang-cl.exe --driver-mode=cl /blah',
    },
    {
        'command': r'C:\clang-cl.exe --driver-mode=cl /blah',
    },
    {
        'command': r'clang-cl.exe --driver-mode=cl /blah '
                   r'"-DCR_CLANG_REVISION=\"346388-1\""',
    },
    {
        'command': r'clang-cl.exe --driver-mode=cl /blah '
                   r'-D"MY_DEFINE=\"MY VALUE\""',
    },
]


class CompileDbTest(unittest.TestCase):

  def setUp(self):
    self.maxDiff = None

  def testProcessNotOnWindows(self):
    sys.platform = 'linux2'
    processed_compile_db = compile_db.ProcessCompileDatabase(
        _TEST_COMPILE_DB, [])

    # Assert no changes were made.
    try:
      # assertItemsEqual is renamed assertCountEqual in Python3.
      self.assertCountEqual(processed_compile_db, _TEST_COMPILE_DB)
    except AttributeError:
      self.assertItemsEqual(processed_compile_db, _TEST_COMPILE_DB)

  def testProcessForWindows_HostPlatformBased(self):
    sys.platform = 'win32'
    processed_compile_db = compile_db.ProcessCompileDatabase(
        _TEST_COMPILE_DB, [])

    # Check each entry individually to improve readability of the output.
    for actual, expected in zip(processed_compile_db, _EXPECTED_COMPILE_DB):
      self.assertDictEqual(actual, expected)

  def testProcessForWindows_TargetOsBased(self):
    sys.platform = 'linux2'
    processed_compile_db = compile_db.ProcessCompileDatabase(_TEST_COMPILE_DB,
                                                             [],
                                                             target_os='win')

    # Check each entry individually to improve readability of the output.
    for actual, expected in zip(processed_compile_db, _EXPECTED_COMPILE_DB):
      self.assertDictEqual(actual, expected)

  def testFrontendArgsFiltered(self):
    sys.platform = 'linux2'
    input_db = [{
        'command':
        r'clang -g -Xclang -fuse-ctor-homing -funroll-loops test.cc'
    }]
    self.assertEquals(compile_db.ProcessCompileDatabase(input_db, []),
                      [{
                          'command': r'clang -g -funroll-loops test.cc'
                      }])

  def testProfileSampleUseFiltered(self):
    sys.platform = 'linux2'
    input_db = [{
        'command':
        r'clang -g -fprofile-sample-use=../path/to.prof -funroll-loops test.cc'
    }]
    self.assertEquals(compile_db.ProcessCompileDatabase(input_db, []),
                      [{
                          'command': r'clang -g -funroll-loops test.cc'
                      }])

  def testFilterArgs(self):
    sys.platform = 'linux2'
    input_db = [{'command': r'clang -g -ffile-compilation-dir=. -O3 test.cc'}]
    self.assertEquals(
        compile_db.ProcessCompileDatabase(
            input_db,
            ['-ffile-compilation-dir=.', '-frandom-flag-that-does-not-exist']),
        [{
            'command': r'clang -g -O3 test.cc'
        }])

  def testRewrapperRemoved(self):
    sys.platform = 'linux2'
    input_db = [{
        'command':
        r'./buildtools/reclient/rewrapper ./bin/clang++ -O3 test.cc',
    }]
    self.assertEquals(compile_db.ProcessCompileDatabase(input_db, []),
                      [{
                          'command': r'./bin/clang++ -O3 test.cc'
                      }])

  def testRewrapperArgsRemoved(self):
    sys.platform = 'linux2'
    input_db = [{
        'command':
        r'./buildtools/reclient/rewrapper'
        r' -cfg=./buildtools/reclient_cfgs/.../rewrapper_linux.cfg'
        r' -exec_root=/chromium/src/'
        r' ./bin/clang++ -O3 test.cc',
    }]
    self.assertEquals(compile_db.ProcessCompileDatabase(input_db, []),
                      [{
                          'command': r'./bin/clang++ -O3 test.cc'
                      }])


if __name__ == '__main__':
  unittest.main()
