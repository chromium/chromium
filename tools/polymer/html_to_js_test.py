#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import html_to_js
import os
import shutil
import tempfile
import unittest


_HERE_DIR = os.path.dirname(__file__)


class HtmlToJsTest(unittest.TestCase):
  def setUp(self):
    self._out_folder = None
    self.maxDiff = None

  def tearDown(self):
    if self._out_folder:
      shutil.rmtree(self._out_folder)

  def _read_out_file(self, file_name):
    assert self._out_folder
    with open(os.path.join(self._out_folder, file_name), 'r') as f:
      return f.read()

  def _run_test(self, js_file, js_file_expected):
    assert not self._out_folder
    self._out_folder = tempfile.mkdtemp(dir=_HERE_DIR)
    html_to_js.main([
        '--in_folder',
        os.path.join(_HERE_DIR, 'tests'),
        '--out_folder',
        self._out_folder,
        '--js_files',
        js_file,
    ])

    actual_js = self._read_out_file(js_file)
    with open(os.path.join(_HERE_DIR, 'tests', js_file_expected), 'r') as f:
      expected_js = f.read()
    self.assertMultiLineEqual(str(expected_js), str(actual_js))

  def testHtmlToJs_Js(self):
    self._run_test('html_to_js/foo.js', 'html_to_js/foo_expected.js')

  def testHtmlToJs_Ts(self):
    self._run_test('html_to_js/foo.ts', 'html_to_js/foo_expected.ts')


if __name__ == '__main__':
  unittest.main()
