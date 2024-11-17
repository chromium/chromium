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
                use_js=False,
                extra_css_files=[]):
    assert not self._out_folder
    self._out_folder = tempfile.mkdtemp(dir=_HERE_DIR)

    args = [
        '--in_folder',
        os.path.join(_HERE_DIR, 'tests'), '--out_folder', self._out_folder,
        '--in_files', css_file
    ] + extra_css_files

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

  def testCssToWrapperStylePolymer(self):
    self._run_test('css_to_wrapper/foo_style.css',
                   'css_to_wrapper/foo_style.css.ts',
                   'css_to_wrapper/expected/foo_style.css.ts')

  def testCssToWrapperStyleLit(self):
    self._run_test('css_to_wrapper/foo_style_lit_only.css',
                   'css_to_wrapper/foo_style_lit_only.css.ts',
                   'css_to_wrapper/expected/foo_style_lit_only.css.ts')

  # Test case where a Polymer style file is generated from the equivalent Lit
  # file.
  def testCssToWrapperStyleCopy(self):
    self._run_test(
        'css_to_wrapper/foo_style_copy.css',
        'css_to_wrapper/foo_style_copy.css.ts',
        'css_to_wrapper/expected/foo_style_copy.css.ts',
        # Need to pass the Lit file as well, to satisfy an
        # assertion in css_to_wrapper.py.
        extra_css_files=['css_to_wrapper/foo_style_copy_lit.css'])

  def testCssToWrapperStyleNoIncludes(self):
    self._run_test('css_to_wrapper/foo_no_includes_style.css',
                   'css_to_wrapper/foo_no_includes_style.css.ts',
                   'css_to_wrapper/expected/foo_no_includes_style.css.ts')

  def testCssToWrapperVarsPolymer(self):
    self._run_test('css_to_wrapper/foo_vars.css',
                   'css_to_wrapper/foo_vars.css.ts',
                   'css_to_wrapper/expected/foo_vars.css.ts')

  def testCssToWrapperVarsLit(self):
    self._run_test('css_to_wrapper/foo_vars_lit_only.css',
                   'css_to_wrapper/foo_vars_lit_only.css.ts',
                   'css_to_wrapper/expected/foo_vars.css.ts')

  def testCssToWrapperMinify(self):
    self._run_test('css_to_wrapper/foo_style.css',
                   'css_to_wrapper/foo_style.css.ts',
                   'css_to_wrapper/expected/foo_style.min.css.ts',
                   minify=True)

  # Test case where a Polymer style file is generated from the equivalent Lit
  # file and minification is turned on.
  def testCssToWrapperStyleCopyMinify(self):
    self._run_test(
        'css_to_wrapper/foo_style_copy.css',
        'css_to_wrapper/foo_style_copy.css.ts',
        'css_to_wrapper/expected/foo_style_copy.min.css.ts',
        minify=True,
        # Need to pass the Lit file as well, to satisfy an
        # assertion in css_to_wrapper.py.
        extra_css_files=['css_to_wrapper/foo_style_copy_lit.css'])

  def testCssToWrapperUseJs(self):
    self._run_test('css_to_wrapper/foo_style.css',
                   'css_to_wrapper/foo_style.css.js',
                   'css_to_wrapper/expected/foo_style.css.ts',
                   use_js=True)

  def testCssToWrapperSchemeRelative(self):
    self._run_test('css_to_wrapper/foo_relative_style.css',
                   'css_to_wrapper/foo_relative_style.css.ts',
                   'css_to_wrapper/expected/foo_relative_style.css.ts')


if __name__ == '__main__':
  unittest.main()
