#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
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
    self.maxDiff = None
    self._out_folder = None

  def tearDown(self):
    if self._out_folder:
      shutil.rmtree(self._out_folder)

  def _read_out_file(self, file_name):
    assert self._out_folder
    with open(os.path.join(self._out_folder, file_name), 'r') as f:
      return f.read()

  def _run_test(self,
                html_file,
                wrapper_file,
                wrapper_file_expected,
                template=None,
                minify=False,
                use_js=False,
                scheme=None):
    assert not self._out_folder
    self._out_folder = tempfile.mkdtemp(dir=_HERE_DIR)
    args = [
        '--in_folder',
        os.path.join(_HERE_DIR, 'tests'), '--out_folder', self._out_folder,
        '--in_files', html_file
    ]

    if template:
      args += ['--template', template]

    if minify:
      args.append('--minify')

    if use_js:
      args.append('--use_js')

    if scheme:
      args += ['--scheme', scheme]

    html_to_wrapper.main(args)

    actual_wrapper = self._read_out_file(wrapper_file)
    with open(os.path.join(_HERE_DIR, 'tests', wrapper_file_expected),
              'r') as f:
      expected_wrapper = f.read()
    self.assertMultiLineEqual(str(expected_wrapper), str(actual_wrapper))

  def testHtmlToWrapperPolymerElement(self):
    self._run_test('html_to_wrapper/foo.html', 'html_to_wrapper/foo.html.ts',
                   'html_to_wrapper/expected/foo.html.ts')

  def testHtmlToWrapperPolymerElement_Detect(self):
    self._run_test('html_to_wrapper/foo.html',
                   'html_to_wrapper/foo.html.ts',
                   'html_to_wrapper/expected/foo.html.ts',
                   template='detect')

  def testHtmlToWrapperLitElement(self):
    self._run_test('html_to_wrapper/foo_lit.html',
                   'html_to_wrapper/foo_lit.html.ts',
                   'html_to_wrapper/expected/foo_lit.html.ts',
                   template='lit')

  def testHtmlToWrapperLitElement_Detect(self):
    self._run_test('html_to_wrapper/foo_lit.html',
                   'html_to_wrapper/foo_lit.html.ts',
                   'html_to_wrapper/expected/foo_lit.html.ts',
                   template='detect')

  def testHtmlToWrapperLitElement_Minify(self):
    self._run_test('html_to_wrapper/foo_lit.html',
                   'html_to_wrapper/foo_lit.html.ts',
                   'html_to_wrapper/expected/foo_lit.html.ts',
                   template='lit',
                   minify=True)

  def testHtmlToWrapperLitElement_WithImports(self):
    self._run_test('html_to_wrapper/foo_lit_with_imports.html',
                   'html_to_wrapper/foo_lit_with_imports.html.ts',
                   'html_to_wrapper/expected/foo_lit_with_imports.html.ts',
                   template='lit')

  def testHtmlToWrapperNativeElement(self):
    self._run_test('html_to_wrapper/foo_native.html',
                   'html_to_wrapper/foo_native.html.ts',
                   'html_to_wrapper/expected/foo_native.html.ts',
                   template='native')

  def testHtmlToWrapperNativeElement_Detect(self):
    self._run_test('html_to_wrapper/foo_native.html',
                   'html_to_wrapper/foo_native.html.ts',
                   'html_to_wrapper/expected/foo_native.html.ts',
                   template='detect')

  def testHtmlToWrapperIcons(self):
    self._run_test('html_to_wrapper/icons.html',
                   'html_to_wrapper/icons.html.ts',
                   'html_to_wrapper/expected/icons.html.ts')

  def testHtmlToWrapperIconsLit(self):
    self._run_test('html_to_wrapper/cr_icons.html',
                   'html_to_wrapper/cr_icons.html.ts',
                   'html_to_wrapper/expected/cr_icons.html.ts',
                   template='lit')

  def testHtmlToWrapperIconsLit_Detect(self):
    self._run_test('html_to_wrapper/cr_icons.html',
                   'html_to_wrapper/cr_icons.html.ts',
                   'html_to_wrapper/expected/cr_icons.html.ts',
                   template='detect')

  def testHtmlToWrapperLitFromPolymer(self):
    self._run_test('html_to_wrapper/icons_lit.html',
                   'html_to_wrapper/icons_lit.html.ts',
                   'html_to_wrapper/expected/cr_icons.html.ts',
                   template='detect')

  def testHtmlToWrapper_Minify(self):
    self._run_test('html_to_wrapper/foo.html',
                   'html_to_wrapper/foo.html.ts',
                   'html_to_wrapper/expected/foo.min.html.ts',
                   minify=True)

  def testHtmlToWrapper_MinifyDetect(self):
    self._run_test('html_to_wrapper/foo.html',
                   'html_to_wrapper/foo.html.ts',
                   'html_to_wrapper/expected/foo.min.html.ts',
                   minify=True,
                   template='detect')

  def testHtmlToWrapper_UseJs(self):
    self._run_test('html_to_wrapper/foo.html',
                   'html_to_wrapper/foo.html.js',
                   'html_to_wrapper/expected/foo.html.ts',
                   use_js=True)

  def testHtmlToWrapper_UseJsDetect(self):
    self._run_test('html_to_wrapper/foo.html',
                   'html_to_wrapper/foo.html.js',
                   'html_to_wrapper/expected/foo.html.ts',
                   use_js=True,
                   template='detect')

  def testHtmlToWrapperSchemeRelative(self):
    self._run_test('html_to_wrapper/foo.html',
                   'html_to_wrapper/foo.html.ts',
                   'html_to_wrapper/expected/foo.html.ts',
                   scheme='relative')

  def testHtmlToWrapperSchemeChrome(self):
    self._run_test('html_to_wrapper/foo.html',
                   'html_to_wrapper/foo.html.ts',
                   'html_to_wrapper/expected/foo_chrome.html.ts',
                   scheme='chrome')


if __name__ == '__main__':
  unittest.main()
