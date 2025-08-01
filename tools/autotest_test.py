#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import io
import os
import unittest
from pathlib import Path

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


if __name__ == '__main__':
  unittest.main()
