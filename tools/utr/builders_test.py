#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for builders.py"""

import json
import os
import pathlib
import shutil
import tempfile
import unittest
from unittest import mock

import builders


class BuilderPropsTests(unittest.TestCase):

  def setUp(self):
    self.tmp_dir = pathlib.Path(tempfile.mkdtemp())

    patch_props_dir = mock.patch('builders._BUILDER_PROP_DIRS', self.tmp_dir)
    mock_props_dir = patch_props_dir.start()
    self.addCleanup(patch_props_dir.stop)

  def tearDown(self):
    shutil.rmtree(self.tmp_dir)

  def testNoProps(self):
    # Empty base dir
    self.assertEqual(builders.find_builder_props('some-bucket', 'some-builder'),
                     None)

    # Empty bucket dir
    os.makedirs(self.tmp_dir.joinpath('some-bucket'))
    self.assertEqual(builders.find_builder_props('some-bucket', 'some-builder'),
                     None)

    # Empty builder dir
    os.makedirs(self.tmp_dir.joinpath('some-bucket', 'some-builder'))
    self.assertEqual(builders.find_builder_props('some-bucket', 'some-builder'),
                     None)

  def testSomeProps(self):
    builder_dir = self.tmp_dir.joinpath('some-bucket', 'some-builder')
    os.makedirs(builder_dir)
    with open(builder_dir.joinpath('properties.json'), 'w') as f:
      json.dump({'some-key': 'some-val'}, f)

    props = builders.find_builder_props('some-bucket', 'some-builder')
    self.assertEqual(props['some-key'], 'some-val')


if __name__ == '__main__':
  unittest.main()
