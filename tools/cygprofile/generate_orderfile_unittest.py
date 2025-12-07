#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import pathlib
import tempfile
import unittest
import unittest.mock

import generate_orderfile


class GenerateOrderfileTest(unittest.TestCase):

  def setUp(self):
    self.temp_dir = tempfile.TemporaryDirectory()
    self.mock_out_dir = pathlib.Path(self.temp_dir.name) / 'out'
    self.mock_out_dir.mkdir()

    self.mock_args = argparse.Namespace(android_browser='test-browser',
                                        out_dir=self.mock_out_dir,
                                        arch='arm64',
                                        profile_webview=False,
                                        streamline_for_debugging=False,
                                        save_profile_data=False,
                                        verbosity=0,
                                        isolated_script_test_output=None)

    self.profile_tool_patch = unittest.mock.patch(
        'android_profile_tool.AndroidProfileTool', autospec=True)
    self.mock_profile_tool_cls = self.profile_tool_patch.start()
    self.mock_profile_tool = self.mock_profile_tool_cls.return_value

    self.get_libchrome_patch = unittest.mock.patch(
        'orderfile_shared.GetLibchromeSoPath',
        return_value=str(self.mock_out_dir / 'lib.unstripped/libchrome.so'))
    self.mock_get_libchrome = self.get_libchrome_patch.start()

    self.collect_profiles_patch = unittest.mock.patch(
        'orderfile_shared.CollectProfiles', return_value=['profile1.prof'])
    self.mock_collect_profiles = self.collect_profiles_patch.start()

    self.process_profiles_patch = unittest.mock.patch(
        'orderfile_shared.ProcessProfiles',
        return_value=(['symbol1', 'symbol2'], 100))
    self.mock_process_profiles = self.process_profiles_patch.start()

    self.add_dummy_funcs_patch = unittest.mock.patch(
        'orderfile_shared.AddDummyFunctions')
    self.mock_add_dummy_funcs = self.add_dummy_funcs_patch.start()

  def tearDown(self):
    self.temp_dir.cleanup()
    self.profile_tool_patch.stop()
    self.get_libchrome_patch.stop()
    self.collect_profiles_patch.stop()
    self.process_profiles_patch.stop()
    self.add_dummy_funcs_patch.stop()

  def test_GenerateOrderfile(self):
    generate_orderfile.GenerateOrderfile(self.mock_args, 'device1')

    self.mock_profile_tool_cls.assert_called_once_with(str(self.mock_out_dir /
                                                           'profile_data'),
                                                       'device1',
                                                       debug=False,
                                                       verbosity=0)

    self.mock_get_libchrome.assert_called_once_with(self.mock_out_dir,
                                                    'arm64', False)
    self.mock_collect_profiles.assert_called_once_with(self.mock_profile_tool,
                                                       False, 'arm64',
                                                       'test-browser',
                                                       str(self.mock_out_dir),
                                                       None)
    self.mock_process_profiles.assert_called_once_with(
        ['profile1.prof'],
        str(self.mock_out_dir / 'lib.unstripped/libchrome.so'))

    unpatched_orderfile = (self.mock_out_dir /
                           'orderfiles/unpatched_orderfile.arm64')
    self.assertTrue(unpatched_orderfile.exists())
    with open(unpatched_orderfile) as f:
      self.assertEqual(f.read(), 'symbol1\nsymbol2')

    self.mock_profile_tool.Cleanup.assert_called_once()

    patched_orderfile = self.mock_out_dir / 'orderfiles/orderfile.arm64.out'
    self.mock_add_dummy_funcs.assert_called_once_with(str(unpatched_orderfile),
                                                      str(patched_orderfile))

  def test_GetOrderfilesDir(self):
    orderfiles_dir = generate_orderfile._GetOrderfilesDir(self.mock_args)
    self.assertEqual(orderfiles_dir, self.mock_out_dir / 'orderfiles')
    self.assertTrue(orderfiles_dir.exists())

  def test_GetOrderfileFilename(self):
    filename = generate_orderfile._GetOrderfileFilename(self.mock_args)
    expected_path = self.mock_out_dir / 'orderfiles/orderfile.arm64.out'
    self.assertEqual(filename, str(expected_path))

  def test_GetUnpatchedOrderfileFilename(self):
    filename = generate_orderfile._GetUnpatchedOrderfileFilename(self.mock_args)
    expected_path = self.mock_out_dir / 'orderfiles/unpatched_orderfile.arm64'
    self.assertEqual(filename, str(expected_path))


if __name__ == '__main__':
  unittest.main()
