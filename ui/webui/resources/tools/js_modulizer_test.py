#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import js_modulizer
import os
import shutil
import tempfile
import unittest


_HERE_DIR = os.path.dirname(__file__)


class JsModulizerTest(unittest.TestCase):
  def setUp(self):
    self._out_folder = None

  def tearDown(self):
    shutil.rmtree(self._out_folder)

  def _read_out_file(self, file_name):
    assert self._out_folder
    return open(os.path.join(self._out_folder, file_name), 'rb').read()

  def _run_test_(self, js_file, js_file_expected, namespace_rewrites=None):
    assert not self._out_folder
    self._out_folder = tempfile.mkdtemp(dir=_HERE_DIR)
    args = [
      '--input_files', js_file,
      '--in_folder', os.path.join(_HERE_DIR, 'tests'),
      '--out_folder', self._out_folder,
    ]
    if namespace_rewrites:
      args += ['--namespace_rewrites'] + namespace_rewrites

    js_modulizer.main(args)

    js_out_file = os.path.basename(js_file).replace('.js', '.m.js')
    actual_js = self._read_out_file(js_out_file)
    expected_js = open(
        os.path.join(_HERE_DIR, 'tests', js_file_expected), 'rb').read()
    self.assertEquals(expected_js, actual_js)

  def testSuccess_WithoutCrDefine(self):
    self._run_test_('without_cr_define.js', 'without_cr_define_expected.js')

  def testSuccess_WithCrDefine(self):
    self._run_test_('with_cr_define.js', 'with_cr_define_expected.js')

  def testSuccess_WithRename(self):
    self._run_test_(
        'with_rename.js', 'with_rename_expected.js', ['cr.foo.Bar|Bar'])

  def testSuccess_WithIgnore(self):
    self._run_test_('with_ignore.js', 'with_ignore_expected.js')

if __name__ == '__main__':
  unittest.main()
