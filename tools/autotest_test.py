#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import io
import os
import unittest
from pathlib import Path
from unittest import mock

from pyfakefs.fake_filesystem_unittest import TestCase

import autotest


class FindMatchingTestFilesTest(TestCase):

  def setUp(self):
    super().setUp()
    self.setUpPyfakefs()

  def create_cc_test(self, path: os.PathLike):
    self.fs.create_file(
        path, contents='#include "testing/gtest/include/gtest/gtest.h"\n')

  def test_cc_test(self):
    self.create_cc_test('foo_unittest.cc')
    self.assertEqual(['foo_unittest.cc'],
                     autotest.FindMatchingTestFiles('foo_unittest.cc'))

  def test_mm_test(self):
    self.create_cc_test('foo_unittest.mm')
    self.assertEqual(['foo_unittest.mm'],
                     autotest.FindMatchingTestFiles('foo_unittest.mm'))

  def test_cc_alt_test(self):
    self.fs.create_file('foo.cc')
    self.create_cc_test('foo_unittest.cc')
    self.assertEqual(['foo_unittest.cc'],
                     autotest.FindMatchingTestFiles('foo.cc'))

  def test_cc_maybe_test(self):
    self.fs.create_file('foo_unittest.cc')
    self.assertEqual(['foo_unittest.cc'],
                     autotest.FindMatchingTestFiles('foo_unittest.cc'))

  def test_cc_alt_maybe_test(self):
    self.fs.create_file('foo.cc')
    self.fs.create_file('foo_unittest.cc')
    self.assertEqual(['foo_unittest.cc'],
                     autotest.FindMatchingTestFiles('foo.cc'))

  def test_cc_no_test(self):
    self.fs.create_file('foo.cc')
    stderr_buf = io.StringIO()
    with contextlib.redirect_stderr(stderr_buf):
      with self.assertRaises(SystemExit):
        autotest.FindMatchingTestFiles('foo.cc')
    self.assertRegex(stderr_buf.getvalue(),
                     "foo.cc doesn't look like a test file")

  def test_h_for_cc_test(self):
    self.fs.create_file('foo.h')
    self.create_cc_test('foo_unittest.cc')
    self.assertEqual(['foo_unittest.cc'],
                     autotest.FindMatchingTestFiles('foo.h'))

  def test_h_for_mm_test(self):
    self.fs.create_file('foo.h')
    self.create_cc_test('foo_unittest.mm')
    self.assertEqual(['foo_unittest.mm'],
                     autotest.FindMatchingTestFiles('foo.h'))

  def test_java(self):
    self.fs.create_file('Foo.java')
    self.assertEqual(['Foo.java'], autotest.FindMatchingTestFiles('Foo.java'))


class FindTestTargetsTest(unittest.TestCase):

  def setUp(self):
    self.mock_run_command = mock.patch('autotest.RunCommand').start()
    self.addCleanup(mock.patch.stopall)

  def test_mixed_targets(self):
    # Simulate `gn refs` output for the command:
    # $ gn refs out_/Default --all --relation=source --relation=input \
    #     chrome/browser/ui/browser_browsertest.cc \
    #     third_party/blink/renderer/platform/wtf/vector_test.cc
    self.mock_run_command.return_value = """
//:blink_tests
//:gn_all
//chrome/test:browser_tests
//chrome/test:performance_browser_tests
//third_party/blink/public:all_blink
//third_party/blink/renderer/platform/wtf:wtf_unittests
//third_party/blink/renderer/platform/wtf:wtf_unittests_sources
"""
    mock_cache = mock.Mock()
    mock_cache.Find.return_value = []  # Cache miss.
    mock_args = mock.Mock()

    targets, used_cache = autotest.FindTestTargets(mock_cache, 'out/Default', [
        'chrome/browser/ui/browser_browsertest.cc',
        'third_party/blink/renderer/platform/wtf/vector_test.cc'
    ], mock_args)

    # Verify that both targets are identified.
    self.assertIn('chrome/test:browser_tests', targets)
    self.assertIn('third_party/blink/renderer/platform/wtf:wtf_unittests',
                  targets)


if __name__ == '__main__':
  unittest.main()
