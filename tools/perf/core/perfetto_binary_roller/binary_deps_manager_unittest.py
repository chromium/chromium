# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import shutil
import tempfile
import unittest

from core.perfetto_binary_roller import binary_deps_manager

import mock


class BinaryDepsManagerTests(unittest.TestCase):
  def setUp(self):
    self.temp_dir = tempfile.mkdtemp()
    self.config_path = os.path.join(self.temp_dir, 'config.json')
    self.original_config_path = binary_deps_manager.CONFIG_PATH
    binary_deps_manager.CONFIG_PATH = self.config_path

  def tearDown(self):
    binary_deps_manager.CONFIG_PATH = self.original_config_path
    shutil.rmtree(self.temp_dir)

  def writeConfig(self, config):
    with open(self.config_path, 'w') as f:
      json.dump(config, f)

  def readConfig(self):
    with open(self.config_path) as f:
      return json.load(f)

  def testUploadHostBinary(self):
    with mock.patch('py_utils.cloud_storage.Exists') as exists_patch:
      with mock.patch('py_utils.cloud_storage.Insert') as insert_patch:
        with mock.patch('py_utils.GetHostOsName') as get_os_patch:
          exists_patch.return_value = False
          get_os_patch.return_value = 'testos'
          binary_deps_manager.UploadHostBinary('dep', '/path/to/bin', 'abc123')

    insert_patch.assert_has_calls([
        mock.call(
            'chromium-telemetry',
            'perfetto_binaries/dep/testos/abc123/bin',
            '/path/to/bin',
            publicly_readable=True),
        mock.call(
            'chromium-telemetry',
            'perfetto_binaries/dep/testos/latest',
            mock.ANY,
            publicly_readable=True),
    ])

  def testUploadHostBinaryExists(self):
    with mock.patch('py_utils.cloud_storage.Exists') as exists_patch:
      with mock.patch('py_utils.cloud_storage.Insert') as insert_patch:
        with mock.patch('py_utils.GetHostOsName') as get_os_patch:
          exists_patch.return_value = True
          get_os_patch.return_value = 'testos'
          binary_deps_manager.UploadHostBinary('dep', '/path/to/bin', 'abc123')

    insert_patch.assert_called_once_with(
        'chromium-telemetry',
        'perfetto_binaries/dep/testos/latest',
        mock.ANY,
        publicly_readable=True,
    )

  def testSwitchBinaryToNewPath(self):
    self.writeConfig({'dep': {'testos': {'remote_path': 'old/path/to/bin'}}})
    latest_path = 'new/path/to/bin'

    def write_latest_path(bucket, remote_path, local_path):
      del bucket, remote_path  # unused
      with open(local_path, 'w') as f:
        f.write(latest_path)

    with mock.patch('py_utils.cloud_storage.Get') as get_patch:
      with mock.patch('py_utils.cloud_storage.CalculateHash') as hash_patch:
        get_patch.side_effect = write_latest_path
        hash_patch.return_value = '123'
        binary_deps_manager.SwitchBinaryToNewPath('dep', 'testos', latest_path)

    self.assertEqual(
        self.readConfig(),
        {'dep': {
            'testos': {
                'remote_path': latest_path,
                'hash': '123',
            }
        }})

  def testFetchHostBinary(self):
    remote_path = 'remote/path/to/bin'
    self.writeConfig({
        'dep': {
            'testos': {
                'remote_path': remote_path,
                'hash': '123',
            }
        }
    })
    with mock.patch('py_utils.cloud_storage.Get') as get_patch:
      with mock.patch('py_utils.GetHostOsName') as get_os_patch:
        with mock.patch('py_utils.cloud_storage.CalculateHash') as hash_patch:
          with mock.patch('os.stat'):
            with mock.patch('os.chmod'):
              hash_patch.return_value = '123'
              get_os_patch.return_value = 'testos'
              local_path = binary_deps_manager.FetchHostBinary('dep')

    self.assertEqual(os.path.basename(local_path), 'bin')
    get_patch.assert_called_once_with('chromium-telemetry', remote_path,
                                      local_path)

  def testFetchHostBinaryWrongHash(self):
    remote_path = 'remote/path/to/bin'
    self.writeConfig({
        'dep': {
            'testos': {
                'remote_path': remote_path,
                'hash': '123',
            }
        }
    })
    with mock.patch('py_utils.cloud_storage.Get'):
      with mock.patch('py_utils.GetHostOsName') as get_os_patch:
        with mock.patch('py_utils.cloud_storage.CalculateHash') as hash_patch:
          hash_patch.return_value = '234'
          get_os_patch.return_value = 'testos'
          with self.assertRaises(RuntimeError):
            binary_deps_manager.FetchHostBinary('dep')

  def testUploadAndSwitchDataFile(self):
    self.writeConfig({'data_dep': {'remote_path': 'old/path/to/data'}})
    new_path = 'new/path/to/data'

    with mock.patch('py_utils.cloud_storage.Exists') as exists_patch:
      with mock.patch('py_utils.cloud_storage.Insert') as insert_patch:
        with mock.patch('py_utils.cloud_storage.CalculateHash') as hash_patch:
          exists_patch.return_value = False
          hash_patch.return_value = '123'
          binary_deps_manager.UploadAndSwitchDataFile('data_dep', new_path,
                                                      'abc123')

    insert_patch.assert_called_once_with(
        'chrome-telemetry',
        'perfetto_data/data_dep/abc123/data',
        'new/path/to/data',
        publicly_readable=False,
    )

    self.assertEqual(
        self.readConfig(), {
            'data_dep': {
                'remote_path': 'perfetto_data/data_dep/abc123/data',
                'hash': '123',
            }
        })

  def testFetchDataFile(self):
    remote_path = 'remote/path/to/data'
    self.writeConfig(
        {'data_dep': {
            'remote_path': remote_path,
            'hash': '123',
        }})
    with mock.patch('py_utils.cloud_storage.Get') as get_patch:
      with mock.patch('py_utils.cloud_storage.CalculateHash') as hash_patch:
        hash_patch.return_value = '123'
        local_path = binary_deps_manager.FetchDataFile('data_dep')

    self.assertEqual(os.path.basename(local_path), 'data')
    get_patch.assert_called_once_with('chrome-telemetry', remote_path,
                                      local_path)
