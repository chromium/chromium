#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for builder_utils.py"""

import unittest
from unittest import mock
import pathlib

import builder_utils


class BuilderUtilsTests(unittest.TestCase):

  @mock.patch('builder_utils.gn_helpers.ReadArgsGN')
  def testReadExplicitGnArgs(self, mock_read_args):
    mock_read_args.return_value = {'some_arg': 'some_val'}
    build_dir = pathlib.Path('/some/build/dir')
    args = builder_utils.read_explicit_gn_args(build_dir)
    mock_read_args.assert_called_once_with(str(build_dir))
    self.assertEqual(args, {'some_arg': 'some_val'})

  @mock.patch('builder_utils.detect_host_arch.HostArch')
  @mock.patch('builder_utils.platform.system')
  def testGetHostFallbackArgs(self, mock_system, mock_host_arch):
    mock_host_arch.return_value = 'x64'

    mock_system.return_value = 'Linux'
    os_name, cpu = builder_utils.get_host_fallback_args()
    self.assertEqual(os_name, 'linux')
    self.assertEqual(cpu, 'x64')

    mock_system.return_value = 'Darwin'
    os_name, cpu = builder_utils.get_host_fallback_args()
    self.assertEqual(os_name, 'mac')

    mock_system.return_value = 'Windows'
    os_name, cpu = builder_utils.get_host_fallback_args()
    self.assertEqual(os_name, 'win')

  @mock.patch('builder_utils.get_host_fallback_args')
  @mock.patch('builder_utils.read_explicit_gn_args')
  def testGuessBuilder(self, mock_read_args, mock_fallback):
    # Case 1: No GN args, fall back
    mock_read_args.return_value = {}
    mock_fallback.return_value = ('linux', 'x64')
    builder = builder_utils.guess_builder('/some/dir')
    self.assertEqual(builder, ('ci', 'Linux Tests'))

    # Case 2: GN args present
    mock_read_args.return_value = {
        'target_os': 'android',
        'target_cpu': 'arm64'
    }
    builder = builder_utils.guess_builder('/some/dir')
    self.assertEqual(builder, ('ci', 'android-14-arm64-rel'))

    # Case 3: Unknown combo
    mock_read_args.return_value = {
        'target_os': 'unknown',
        'target_cpu': 'unknown'
    }
    builder = builder_utils.guess_builder('/some/dir')
    self.assertIsNone(builder)


if __name__ == '__main__':
  unittest.main()
