#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import minify_js
import os
import tempfile
import shutil
import unittest

_HERE_DIR = os.path.dirname(__file__)


class MinifyJsTest(unittest.TestCase):

  def setUp(self):
    self._out_dir = tempfile.mkdtemp(dir=_HERE_DIR)

  def tearDown(self):
    shutil.rmtree(self._out_dir)

  def _read_file(self, path):
    with open(path, 'r', encoding='utf-8') as file:
      return file.read()

  def _run_test(self, in_files, expected_files):
    manifest_path = os.path.join(self._out_dir, "manifest.json")
    args = [
        "--in_folder",
        os.path.join(_HERE_DIR, "tests", "minify_js"),
        "--out_folder",
        self._out_dir,
        "--out_manifest",
        manifest_path,
        "--in_files",
        *in_files,
    ]

    minify_js.main(args)

    manifest_data = json.loads(self._read_file(manifest_path))

    self.assertEqual(manifest_data['base_dir'], self._out_dir)

    for index, in_file in enumerate(in_files):
      actual_contents = self._read_file(os.path.join(self._out_dir, in_file))
      expected_contents = self._read_file(expected_files[index])
      self.assertMultiLineEqual(expected_contents, actual_contents)
      self.assertTrue(in_file in manifest_data['files'])

  def testMinifySimpleFile(self):
    self._run_test(["foo.js"], ["tests/minify_js/foo_expected.js"])

  def testMinifyComplexFile(self):
    self._run_test(["bar.js"], ["tests/minify_js/bar_expected.js"])

  def testMinifyMultipleFiles(self):
    self._run_test(
        ["foo.js", "bar.js"],
        ["tests/minify_js/foo_expected.js", "tests/minify_js/bar_expected.js"])


if __name__ == "__main__":
  unittest.main()
