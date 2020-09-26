#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import generate_grd
import os
import shutil
import tempfile
import unittest


_CWD = os.getcwd()
_HERE_DIR = os.path.dirname(__file__)
pathToHere = os.path.relpath(_HERE_DIR, _CWD)


class GenerateGrdTest(unittest.TestCase):
  def setUp(self):
    self._out_folder = None

  def tearDown(self):
    shutil.rmtree(self._out_folder)

  def _read_out_file(self, file_name):
    assert self._out_folder
    return open(os.path.join(self._out_folder, file_name), 'rb').read()

  def _run_test_(self, manifest_files, grd_expected):
    assert not self._out_folder
    self._out_folder = tempfile.mkdtemp(dir=_HERE_DIR)
    args = [
      '--out-grd', os.path.join(self._out_folder, 'test_resources.grd'),
      '--grd-prefix', 'test',
      '--root-gen-dir', os.path.join(_CWD, pathToHere, 'tests'),
      '--manifest-files',
    ] + manifest_files

    generate_grd.main(args)

    actual_grd = self._read_out_file('test_resources.grd')
    expected_grd = open(
        os.path.join(_HERE_DIR, 'tests', grd_expected), 'rb').read()
    self.assertEquals(expected_grd, actual_grd)

  def testSuccess(self):
    self._run_test_([
        os.path.join(pathToHere, 'tests', 'test_manifest_1.json'),
        os.path.join(pathToHere, 'tests', 'test_manifest_2.json'),
      ], 'expected_grd.grd')

if __name__ == '__main__':
  unittest.main()
