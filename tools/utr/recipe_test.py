#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for recipe.py"""

import os
import pathlib
import shutil
import tempfile
import unittest
from unittest import mock

import recipe


class LegacyRunnerTests(unittest.TestCase):

  def setUp(self):
    self.tmp_dir = pathlib.Path(tempfile.mkdtemp())
    self.tmp_dir.joinpath('recipes').touch()
    self.subp_mock = mock.Mock()

  def tearDown(self):
    shutil.rmtree(self.tmp_dir)

  def testProps(self):
    runner = recipe.LegacyRunner(self.tmp_dir, {}, 'some-bucket',
                                 'some-builder', [], False, False)
    self.assertEqual(
        runner._input_props['$recipe_engine/buildbucket']['build']['builder']
        ['builder'], 'some-builder')

  def testRun(self):
    runner = recipe.LegacyRunner(self.tmp_dir, {}, 'some-bucket',
                                 'some-builder', [], False, False)
    self.subp_mock.returncode = 123
    with mock.patch('subprocess.Popen', return_value=self.subp_mock):
      self.assertEqual(runner.run_recipe(), 123)


if __name__ == '__main__':
  unittest.main()
