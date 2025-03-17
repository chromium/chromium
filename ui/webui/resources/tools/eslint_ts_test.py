#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import eslint_ts
import os
import tempfile
import shutil
import unittest

_HERE_DIR = os.path.dirname(__file__)


class EslintTsTest(unittest.TestCase):

  _in_folder = os.path.join(_HERE_DIR, "tests", "eslint_ts")

  def setUp(self):
    self._out_dir = tempfile.mkdtemp(dir=self._in_folder)

  def tearDown(self):
    shutil.rmtree(self._out_dir)

  def _read_file(self, path):
    with open(path, "r", encoding="utf-8") as file:
      return file.read()

  def _run_test(self, in_files):
    config_base = os.path.join(_HERE_DIR, "eslint_ts.config_base.mjs")
    tsconfig = os.path.join(self._in_folder, "tsconfig.json")

    args = [
        "--in_folder",
        self._in_folder,
        "--out_folder",
        self._out_dir,
        "--config_base",
        os.path.relpath(config_base, self._out_dir).replace(os.sep, '/'),
        "--tsconfig",
        os.path.relpath(tsconfig, self._out_dir).replace(os.sep, '/'),
        "--in_files",
        *in_files,
    ]

    eslint_ts.main(args)

  def testSuccess(self):
    self._run_test(["no_violations.ts"])
    actual_contents = self._read_file(
        os.path.join(self._out_dir, "eslint.config.mjs"))
    expected_contents = self._read_file(
        os.path.join(self._in_folder, "eslint_expected.config.mjs"))
    self.assertMultiLineEqual(expected_contents, actual_contents)

  def testError(self):
    with self.assertRaises(RuntimeError) as context:
      self._run_test(["with_violations.ts"])

    # Expected ESLint rule violation that should be part of the error output.
    _EXPECTED_STRING = "@typescript-eslint/require-await"
    self.assertTrue(_EXPECTED_STRING in str(context.exception))


if __name__ == "__main__":
  unittest.main()
