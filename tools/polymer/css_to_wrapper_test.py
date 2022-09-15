#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import css_to_wrapper
import os
import shutil
import tempfile
import unittest

_HERE_DIR = os.path.dirname(__file__)


class CssToWrapperTest(unittest.TestCase):
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

  def _run_test(self,
                css_file,
                wrapper_file,
                wrapper_file_expected,
                minify=False,
                use_js=False):
    assert not self._out_folder
    self._out_folder = tempfile.mkdtemp(dir=_HERE_DIR)
    args = [
        '--in_folder',
        os.path.join(_HERE_DIR, 'tests'), '--out_folder', self._out_folder,
        '--in_files', css_file
    ]

    if minify:
      args.append('--minify')

    if use_js:
      args.append('--use_js')

    css_to_wrapper.main(args)

    actual_wrapper = self._read_out_file(wrapper_file)
    with open(os.path.join(_HERE_DIR, 'tests', wrapper_file_expected),
              'r') as f:
      expected_wrapper = f.read()

    self.assertMultiLineEqual(str(expected_wrapper), str(actual_wrapper))

  def testCssToWrapperStyle(self):
    self._run_test('css_to_wrapper/foo_style.css',
                   'css_to_wrapper/foo_style.css.ts',
                   'css_to_wrapper/foo_style_expected.css.ts')

  def testCssToWrapperStyleNoIncludes(self):
    self._run_test('css_to_wrapper/foo_no_includes_style.css',
                   'css_to_wrapper/foo_no_includes_style.css.ts',
                   'css_to_wrapper/foo_no_includes_style_expected.css.ts')

  def testCssToWrapperVars(self):
    self._run_test('css_to_wrapper/foo_vars.css',
                   'css_to_wrapper/foo_vars.css.ts',
                   'css_to_wrapper/foo_vars_expected.css.ts')

  def testCssToWrapperMinify(self):
    self._run_test('css_to_wrapper/foo_style.css',
                   'css_to_wrapper/foo_style.css.ts',
                   'css_to_wrapper/foo_style_expected.min.css.ts',
                   minify=True)

  def testCssToWrapperUseJs(self):
    self._run_test('css_to_wrapper/foo_style.css',
                   'css_to_wrapper/foo_style.css.js',
                   'css_to_wrapper/foo_style_expected.css.ts',
                   use_js=True)

  def testCssToWrapperSchemeRelative(self):
    self._run_test('css_to_wrapper/foo_relative_style.css',
                   'css_to_wrapper/foo_relative_style.css.ts',
                   'css_to_wrapper/foo_relative_style_expected.css.ts')


if __name__ == '__main__':
  unittest.main()
