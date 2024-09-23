#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for cipd.py"""

import pathlib
import subprocess
import tempfile
import unittest
from unittest import mock

import cipd


class FetchBundleTest(unittest.TestCase):

  def setUp(self):
    self.tmp_dir = pathlib.Path(tempfile.mkdtemp())
    patch_cipd_dir = mock.patch('cipd._CIPD_ROOT_BASE_DIR', self.tmp_dir)
    patch_cipd_dir.start()
    self.addCleanup(patch_cipd_dir.stop)

    self.subp_mock = mock.MagicMock()
    patch_subp = mock.patch('subprocess.check_call', self.subp_mock)
    patch_subp.start()
    self.addCleanup(patch_subp.stop)

  def testBasic(self):
    bundle_dir = cipd.fetch_recipe_bundle('chromium', True)
    self.assertEqual(bundle_dir, self.tmp_dir.joinpath('chromium'))

    bundle_dir = cipd.fetch_recipe_bundle('chrome', True)
    self.assertEqual(bundle_dir, self.tmp_dir.joinpath('chrome'))

  def testUnknownProject(self):
    # Unknown project defaults to chrome for safety.
    bundle_dir = cipd.fetch_recipe_bundle('unknown-project', True)
    self.assertIn(cipd._CHROME_RECIPE_BUNDLE, self.subp_mock.call_args.args[0])
    self.assertEqual(bundle_dir, self.tmp_dir.joinpath('unknown-project'))


if __name__ == '__main__':
  unittest.main()
