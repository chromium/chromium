#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import generate_code_cache
import json
import os
import shutil
import sys
import tempfile
import unittest

_HERE_DIR = os.path.dirname(__file__)
_BUILD_DIR = os.environ['CHROMIUM_BUILD_DIRECTORY']


class GenerateCodeCacheTest(unittest.TestCase):

  def setUp(self):
    self._out_dir = tempfile.mkdtemp(dir=_HERE_DIR)

  def tearDown(self):
    shutil.rmtree(self._out_dir)

  def _read_file(self, path):
    with open(path, 'r', encoding='utf-8') as file:
      return file.read()

  def _run_test(self, in_files):
    manifest_path = os.path.join(self._out_dir, "manifest.json")
    util_path = os.path.join(_HERE_DIR, "tests", "generate_code_cache",
                             "code_cache_generator_test_util.py")
    args = [
        "--util_path",
        os.path.join(_BUILD_DIR, "code_cache_generator"),
        "--in_folder",
        os.path.join(_HERE_DIR, "tests", "generate_code_cache"),
        "--in_files",
        *in_files,
        "--out_folder",
        self._out_dir,
        "--out_manifest",
        manifest_path,
    ]

    generate_code_cache.main(args)

    manifest_data = json.loads(self._read_file(manifest_path))
    self.assertEqual(manifest_data['base_dir'], self._out_dir)

    for in_file in in_files:
      out_code_cache_file = f'{in_file}.code_cache'
      self.assertTrue(
          os.path.isfile(os.path.join(self._out_dir, out_code_cache_file)))
      self.assertTrue(out_code_cache_file in manifest_data['files'])

  def testGenerateCodeCacheModuleBasic(self):
    self._run_test(["module_basic.js"])

  def testGenerateCodeCacheModuleWithDeps(self):
    self._run_test(["module_with_deps.js"])

  def testGenerateCodeCacheMultipleModules(self):
    self._run_test(["module_basic.js", "module_with_deps.js"])


if __name__ == "__main__":
  unittest.main()
