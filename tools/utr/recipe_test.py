#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for recipe.py"""

import json
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
      exit_code, _ = runner.run_recipe()
      self.assertEqual(exit_code, 123)

  def testJson(self):
    runner = recipe.LegacyRunner(self.tmp_dir, {}, 'some-bucket',
                                 'some-builder', [], False, False)
    with mock.patch('tempfile.TemporaryDirectory', return_value=self.tmp_dir):
      with mock.patch('subprocess.Popen', return_value=self.subp_mock):
        # Missing json file
        _, error_msg = runner.run_recipe()
        self.assertIsNone(error_msg)

        # Broken json
        with open(self.tmp_dir.joinpath('out.json'), 'w') as f:
          f.write('this-is-not-json')
        _, error_msg = runner.run_recipe()
        self.assertIsNone(error_msg)

        # Actual json
        with open(self.tmp_dir.joinpath('out.json'), 'w') as f:
          json.dump({'failure': {'humanReason': 'it exploded'}}, f)
        _, error_msg = runner.run_recipe()
        self.assertEqual(error_msg, 'it exploded')


if __name__ == '__main__':
  unittest.main()
