#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import html_to_wrapper
import os
import shutil
import tempfile
import unittest

_HERE_DIR = os.path.dirname(__file__)


class HtmlToWrapperTest(unittest.TestCase):
  def setUp(self):
    self._out_folder = None

  def tearDown(self):
    if self._out_folder:
      shutil.rmtree(self._out_folder)

  def _read_out_file(self, file_name):
    assert self._out_folder
    with open(os.path.join(self._out_folder, file_name), 'rb') as f:
      return f.read()

  def _run_test(self, html_file, wrapper_file, wrapper_file_expected):
    assert not self._out_folder
    self._out_folder = tempfile.mkdtemp(dir=_HERE_DIR)
    html_to_wrapper.main([
        '--in_folder',
        os.path.join(_HERE_DIR, 'tests'), '--out_folder', self._out_folder,
        '--in_files', html_file
    ])

    actual_wrapper = self._read_out_file(wrapper_file)
    with open(os.path.join(_HERE_DIR, 'tests', wrapper_file_expected),
              'rb') as f:
      expected_wrapper = f.read()
    self.assertMultiLineEqual(str(expected_wrapper), str(actual_wrapper))

  def testHtmlToWrapperElement(self):
    self._run_test('html_to_wrapper/foo.html', 'html_to_wrapper/foo.html.ts',
                   'html_to_wrapper/foo_expected.html.ts')

  def testHtmlToWrapperIcons(self):
    self._run_test('html_to_wrapper/icons.html',
                   'html_to_wrapper/icons.html.ts',
                   'html_to_wrapper/icons_expected.html.ts')


if __name__ == '__main__':
  unittest.main()
