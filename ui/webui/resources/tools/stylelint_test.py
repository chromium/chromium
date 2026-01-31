#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import stylelint
import os
import tempfile
import shutil
import unittest

_HERE_DIR = os.path.dirname(__file__)


class StylelintTest(unittest.TestCase):

  _in_folder = os.path.join(_HERE_DIR, "tests", "stylelint")

  def setUp(self):
    self._out_dir = tempfile.mkdtemp(dir=self._in_folder)

  def tearDown(self):
    shutil.rmtree(self._out_dir)

  def _read_file(self, path):
    with open(path, "r", encoding="utf-8") as file:
      return file.read()

  def _run_test(self, in_files):
    config = os.path.join(_HERE_DIR, "stylelint.config_base.mjs")

    args = [
        "--in_folder",
        self._in_folder,
        "--out_file",
        os.path.join(self._out_dir, 'stylelint_result.txt'),
        "--config",
        config,
        "--in_files",
        *in_files,
    ]

    stylelint.main(args)

  def testSuccess(self):
    self._run_test(["no_violations.css"])
    actual_contents = self._read_file(
        os.path.join(self._out_dir, "stylelint_result.txt"))
    self.assertMultiLineEqual("OK", actual_contents)

  def testError(self):
    with self.assertRaises(RuntimeError) as context:
      self._run_test(["with_violations.css"])

    # Expected rule violation that should be part of the error output.
    _EXPECTED_STRING = "block-no-empty"
    self.assertTrue(_EXPECTED_STRING in str(context.exception))


if __name__ == "__main__":
  unittest.main()
