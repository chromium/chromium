# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import tempfile
import unittest

import preprocess_if_expr

_HERE_DIR = os.path.dirname(__file__)


class PreprocessIfExprTest(unittest.TestCase):
  def setUp(self):
    self._out_folder = None

  def tearDown(self):
    if self._out_folder:
      shutil.rmtree(self._out_folder)

  def _read_out_file(self, file_name):
    assert self._out_folder
    with open(os.path.join(self._out_folder, file_name)) as f:
      return f.read()

  def _run_test(self, additional_options, file_name, expected_file_name):
    assert not self._out_folder
    self._out_folder = tempfile.mkdtemp(dir=_HERE_DIR)
    preprocess_if_expr.main([
        '--in-folder',
        os.path.join(_HERE_DIR, 'preprocess_tests'),
        '--out-folder',
        self._out_folder,
        '--in-files',
        file_name,
    ] + additional_options)
    actual = self._read_out_file(file_name)
    with open(os.path.join(_HERE_DIR, 'preprocess_tests', expected_file_name)) as f:
      expected = f.read()
      self.assertMultiLineEqual(expected, actual)

  def testPreprocess(self):
    self._run_test(
        ['-D', 'foo', '-D', 'bar', '-D', 'apple=false', '-D', 'orange=false'],
        'test_with_ifexpr.js', 'test_with_ifexpr_expected.js')

  def testPreprocessWithComments(self):
    self._run_test([
        '-D', 'foo', '-D', 'bar', '-D', 'apple=false', '-D', 'orange=false',
        '--enable_removal_comments'
    ], 'test_with_ifexpr.js', 'test_with_ifexpr_expected_comments.js')

  def testPreprocessTypescriptWithComments(self):
    self._run_test([
        '-D', 'foo', '-D', 'bar', '-D', 'orange=false',
        '--enable_removal_comments'
    ], 'test_with_ifexpr.ts', 'test_with_ifexpr_expected.ts')

  def testPreprocessHtmlWithComments(self):
    self._run_test(
        ['-D', 'foo', '-D', 'orange=false', '--enable_removal_comments'],
        'test_with_ifexpr.html', 'test_with_ifexpr_expected.html')

  def testPreprocessJavaScriptHtmlTemplateWithComments(self):
    self._run_test(
        ['-D', 'foo', '-D', 'bar=false', '--enable_removal_comments'],
        'test_with_ifexpr.html.js', 'test_with_ifexpr_expected.html.js')

  def testPreprocessTypeScriptHtmlTemplateWithComments(self):
    self._run_test(
        ['-D', 'foo', '-D', 'bar=false', '--enable_removal_comments'],
        'test_with_ifexpr.html.ts', 'test_with_ifexpr_expected.html.ts')


if __name__ == '__main__':
  unittest.main()
