# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import tempfile
import unittest

import preprocess_grit

_HERE_DIR = os.path.dirname(__file__)


class PreprocessGritTest(unittest.TestCase):
  def setUp(self):
    self._out_folder = None

  def tearDown(self):
    if self._out_folder:
      shutil.rmtree(self._out_folder)

  def _read_out_file(self, file_name):
    assert self._out_folder
    return open(os.path.join(self._out_folder, file_name), 'r').read()

  def _run_test(self, defines, file_name):
    assert not self._out_folder
    self._out_folder = tempfile.mkdtemp(dir=_HERE_DIR)
    preprocess_grit.main([
        '--in-folder',
        os.path.join(_HERE_DIR, 'preprocess_tests'),
        '--out-folder',
        self._out_folder,
        '--in-files',
        file_name,
    ] + defines)

  def testPreprocess(self):
    self._run_test(['-D', 'foo', '-D', 'bar'], 'test_with_ifexpr.js')
    actual = self._read_out_file('test_with_ifexpr.js')
    self.assertIn('I should be included in HTML', actual)
    self.assertIn('I should be included in JS', actual)
    self.assertNotIn('I should be excluded from HTML', actual)
    self.assertNotIn('I should be excluded from JS', actual)


if __name__ == '__main__':
  unittest.main()
