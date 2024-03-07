#!/usr/bin/env vpython3
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

  class AsyncMock(mock.MagicMock):

    async def __call__(self, *args, **kwargs):
      return super(mock.MagicMock, self).__call__(*args, **kwargs)

  def setUp(self):
    self.tmp_dir = pathlib.Path(tempfile.mkdtemp())
    self.tmp_dir.joinpath('recipes').touch()
    self.addCleanup(shutil.rmtree, self.tmp_dir)

    self.subp_mock = self.AsyncMock()

    patch_tempdir = mock.patch('tempfile.TemporaryDirectory')
    self.mock_tempdir = patch_tempdir.start()
    self.mock_tempdir.return_value.__enter__.return_value = self.tmp_dir
    self.addCleanup(patch_tempdir.stop)

    patch_input = mock.patch('builtins.input')
    self.mock_input = patch_input.start()
    self.addCleanup(patch_input.stop)

  def testProps(self):
    runner = recipe.LegacyRunner(self.tmp_dir, {}, 'some-bucket',
                                 'some-builder', 'swarming-server', [], False,
                                 False, False)
    self.assertEqual(
        runner._input_props['$recipe_engine/buildbucket']['build']['builder']
        ['builder'], 'some-builder')

  def testRun(self):
    runner = recipe.LegacyRunner(self.tmp_dir, {}, 'some-bucket',
                                 'some-builder', 'swarming-server', [], False,
                                 False, False)
    self.subp_mock.returncode = 123
    with mock.patch('asyncio.create_subprocess_exec',
                    return_value=self.subp_mock):
      exit_code, _ = runner.run_recipe()
      self.assertEqual(exit_code, 123)

  def testJson(self):
    runner = recipe.LegacyRunner(self.tmp_dir, {}, 'some-bucket',
                                 'some-builder', 'swarming-server', [], False,
                                 False, False)
    with mock.patch('asyncio.create_subprocess_exec',
                    return_value=self.subp_mock):
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

  def testReruns(self):
    runner = recipe.LegacyRunner(self.tmp_dir, {}, 'some-bucket',
                                 'some-builder', 'swarming-server', [], False,
                                 False, False)
    with mock.patch('asyncio.create_subprocess_exec',
                    return_value=self.subp_mock):
      # Input "n" to the first re-run prompt.
      self.mock_input.return_value = 'n'
      with open(self.tmp_dir.joinpath('rerun_props.json'), 'w') as f:
        json.dump({'some-new-prop': 'some-val'}, f)
      _, error_msg = runner.run_recipe()
      self.assertEqual(error_msg, 'User-aborted due to warning')

      # Input "y" to too many re-runs.
      self.mock_input.return_value = 'y'
      with open(self.tmp_dir.joinpath('rerun_props.json'), 'w') as f:
        json.dump({'some-new-prop': 'some-val'}, f)
      _, error_msg = runner.run_recipe()
      self.assertEqual(error_msg, 'Exceeded too many recipe re-runs')

      # Re-running once and succeeding. Need to manage two different tmp dirs,
      # one for each recipe invocations.
      first_tmp_dir = self.tmp_dir
      second_tmp_dir = pathlib.Path(tempfile.mkdtemp())
      self.addCleanup(shutil.rmtree, second_tmp_dir)
      self.mock_input.return_value = 'y'
      with open(first_tmp_dir.joinpath('rerun_props.json'), 'w') as f:
        json.dump({'some-new-prop': 'some-val'}, f)
      self.mock_tempdir.side_effect = [first_tmp_dir, second_tmp_dir]
      _, error_msg = runner.run_recipe()
      self.assertIsNone(error_msg)


  def testRerunsWithForce(self):
    runner = recipe.LegacyRunner(self.tmp_dir, {}, 'some-bucket',
                                 'some-builder', 'swarming-server', [], False,
                                 False, True)
    with mock.patch('asyncio.create_subprocess_exec',
                    return_value=self.subp_mock):
      # Re-running once and succeeding. Need to manage two different tmp dirs,
      # one for each recipe invocations. input() shouldn't be called since we
      # pass --force.
      first_tmp_dir = self.tmp_dir
      second_tmp_dir = pathlib.Path(tempfile.mkdtemp())
      self.addCleanup(shutil.rmtree, second_tmp_dir)
      with open(first_tmp_dir.joinpath('rerun_props.json'), 'w') as f:
        json.dump({'some-new-prop': 'some-val'}, f)
      self.mock_tempdir.side_effect = [first_tmp_dir, second_tmp_dir]
      _, error_msg = runner.run_recipe()
      self.assertIsNone(error_msg)
      self.mock_input.assert_not_called()


if __name__ == '__main__':
  unittest.main()
