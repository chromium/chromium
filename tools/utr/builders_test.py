#!/usr/bin/env vpython3
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
    self.tmp_internal_dir = pathlib.Path(tempfile.mkdtemp())

    patch_props_dir = mock.patch('builders._BUILDER_PROP_DIRS', self.tmp_dir)
    patch_props_dir.start()
    self.addCleanup(patch_props_dir.stop)

    patch_internal_props_dir = mock.patch(
        'builders._INTERNAL_BUILDER_PROP_DIRS', self.tmp_internal_dir)
    patch_internal_props_dir.start()
    self.addCleanup(patch_internal_props_dir.stop)

  def tearDown(self):
    shutil.rmtree(self.tmp_dir)
    if self.tmp_internal_dir.exists():
      shutil.rmtree(self.tmp_internal_dir)

  def testNoProps(self):
    # Empty base dir
    props, _ = builders.find_builder_props('some-builder',
                                           bucket_name='some-bucket')
    self.assertIsNone(props)

    # Empty bucket dir
    os.makedirs(self.tmp_dir.joinpath('some-bucket'))
    props, _ = builders.find_builder_props('some-builder',
                                           bucket_name='some-bucket')
    self.assertIsNone(props)

    # Empty builder dir
    os.makedirs(self.tmp_dir.joinpath('some-bucket', 'some-builder'))
    props, _ = builders.find_builder_props('some-builder',
                                           bucket_name='some-bucket')
    self.assertIsNone(props)

    # No src-internal checkout.
    shutil.rmtree(self.tmp_internal_dir)
    props, _ = builders.find_builder_props('some-builder',
                                           bucket_name='some-bucket')
    self.assertIsNone(props)

  def testSomeProps(self):
    builder_dir = self.tmp_dir.joinpath('some-bucket', 'some-builder')
    os.makedirs(builder_dir)
    with open(builder_dir.joinpath('properties.json'), 'w') as f:
      json.dump({'some-key': 'some-val'}, f)

    props, _ = builders.find_builder_props('some-builder',
                                           bucket_name='some-bucket')
    self.assertEqual(props['some-key'], 'some-val')

  def testBucketName(self):
    # Missing bucket_name should be fine since there's only one builder that
    # matches the bulder name.
    builder_dir = self.tmp_dir.joinpath('some-bucket1', 'some-builder')
    os.makedirs(builder_dir)
    with open(builder_dir.joinpath('properties.json'), 'w') as f:
      json.dump({'some-key': 'some-val'}, f)
    props, _ = builders.find_builder_props('some-builder')
    self.assertEqual(props['some-key'], 'some-val')

    # Missing bucket_name should fail since there's now multiple builders that
    # match the builder name under different projects.
    builder_dir = self.tmp_dir.joinpath('some-bucket2', 'some-builder')
    os.makedirs(builder_dir)
    with open(builder_dir.joinpath('properties.json'), 'w') as f:
      json.dump({'some-key': 'some-val'}, f)
    props, _ = builders.find_builder_props('some-builder')
    self.assertIsNone(props)

  def testInternalProps(self):
    # Create props for a public and internal builder, then fetch those props.
    builder_dir = self.tmp_dir.joinpath('some-bucket', 'some-builder')
    os.makedirs(builder_dir)
    with open(builder_dir.joinpath('properties.json'), 'w') as f:
      json.dump({'some-key': 'some-public-val'}, f)

    internal_builder_dir = self.tmp_internal_dir.joinpath(
        'some-bucket', 'some-builder')
    os.makedirs(internal_builder_dir)
    with open(internal_builder_dir.joinpath('properties.json'), 'w') as f:
      json.dump({'some-key': 'some-internal-val'}, f)

    props, _ = builders.find_builder_props('some-builder',
                                           bucket_name='some-bucket',
                                           project_name='chromium')
    self.assertEqual(props['some-key'], 'some-public-val')

    props, _ = builders.find_builder_props('some-builder',
                                           bucket_name='some-bucket',
                                           project_name='chrome')
    self.assertEqual(props['some-key'], 'some-internal-val')

    # Missing project_name should return the internal builder since that's what
    # we want to default to.
    props, project = builders.find_builder_props('some-builder',
                                                 bucket_name='some-bucket')
    self.assertEqual(props['some-key'], 'some-internal-val')
    self.assertEqual(project, 'chrome')

  @mock.patch("subprocess.run")
  def testBuildbucketFallback(self, mock_subprocess_run):
    mock_p = mock.MagicMock()
    mock_subprocess_run.return_value = mock_p

    # Failed bb RPC.
    mock_p.returncode = 1
    props, _ = builders.find_builder_props('some-builder',
                                           bucket_name='some-bucket',
                                           project_name='some-project')
    self.assertIsNone(props)

    # Empty json from bb RPC.
    mock_p.returncode = 0
    mock_p.stdout = '{}'
    props, _ = builders.find_builder_props('some-builder',
                                           bucket_name='some-bucket',
                                           project_name='some-project')
    self.assertIsNone(props)

    # Successful bb RPC.
    props = {'key1': 1, 'key2': 'val2'}
    mock_p.returncode = 0
    mock_p.stdout = json.dumps({
        'config': {
            'properties': json.dumps(props),
        },
    })
    props, _ = builders.find_builder_props('some-builder',
                                           bucket_name='some-bucket',
                                           project_name='some-project')
    self.assertEqual(props, {'key1': 1, 'key2': 'val2'})


if __name__ == '__main__':
  unittest.main()
