#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import os
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch, ANY

import process_crashreports


class TestProcessCrashreports(unittest.TestCase):

  def setUp(self):
    self.temp_dir = tempfile.mkdtemp()
    self.out_dir = os.path.join(self.temp_dir, 'out')
    self.crashreports_dir = os.path.join(self.out_dir, 'clang-crashreports')
    os.makedirs(self.crashreports_dir)

    # Pre-create standard crash files.
    self.crash_sh = os.path.join(self.crashreports_dir, 'main-c2d3e4.sh')
    self.crash_c = os.path.join(self.crashreports_dir, 'main-c2d3e4.c')
    with open(self.crash_sh, 'w') as f:
      f.write('crash script')
    with open(self.crash_c, 'w') as f:
      f.write('crash source')

  def tearDown(self):
    shutil.rmtree(self.temp_dir)

  @patch('subprocess.check_call')
  @patch('sys.argv', ['process_crashreports.py', '--source', 'test-bot'])
  def test_main_local_crash(self, mock_check_call):
    with patch('process_crashreports.CRASHREPORTS_DIR', self.crashreports_dir):
      # Create mock siso_output (for local build with siso)
      default_dir = os.path.join(self.out_dir, 'Default')
      os.makedirs(default_dir)
      siso_output = os.path.join(default_dir, 'siso_output')
      # In a local crash, there are no auxiliary logs.
      with open(siso_output, 'w') as f:
        f.write("build started...\n")
        f.write("build completed\n")

      process_crashreports.main()

      # We verify that exactly one command was run (gsutil), which also
      # implicitly verifies that siso fetch was NOT called.
      self.assertEqual(mock_check_call.call_count, 1)

      now = datetime.datetime.now()
      expected_dest = ('gs://chrome-clang-crash-reports/v1/%04d/%02d/%02d/'
                       'test-bot-main-c2d3e4.tgz' %
                       (now.year, now.month, now.day))

      # Verify the gsutil command fully
      mock_check_call.assert_called_once_with([
          sys.executable, process_crashreports.GSUTIL, '-q', 'cp', ANY,
          expected_dest
      ])

      # Verify files were deleted
      self.assertFalse(os.path.exists(self.crash_sh))
      self.assertFalse(os.path.exists(self.crash_c))

  @patch('subprocess.check_call')
  @patch('sys.argv', ['process_crashreports.py', '--source', 'test-bot'])
  def test_main_rbe_crash(self, mock_check_call):
    with patch('process_crashreports.CRASHREPORTS_DIR', self.crashreports_dir):
      # Create mock siso_output
      default_dir = os.path.join(self.out_dir, 'Default')
      os.makedirs(default_dir)
      siso_output = os.path.join(default_dir, 'siso_output')

      digest = "deadbeef/123"
      fetch_cmd = (f"siso fetch -reapi_instance=test-instance "
                   f"-type=dir-extract {digest} out/clang-crashreports/")
      with open(siso_output, 'w') as f:
        f.write("auxiliary outputs:\n")
        f.write(f"out/clang-crashreports/\t{digest}\t{fetch_cmd}\n")

      process_crashreports.main()

      # Verify siso fetch and gsutil were called
      self.assertEqual(mock_check_call.call_count, 2)

      now = datetime.datetime.now()
      expected_dest = ('gs://chrome-clang-crash-reports/v1/%04d/%02d/%02d/'
                       'test-bot-main-c2d3e4.tgz' %
                       (now.year, now.month, now.day))

      # Verify exact siso command
      mock_check_call.assert_any_call([
          process_crashreports.SISO_BINARY, 'fetch',
          '-reapi_instance=test-instance', '-type=dir-extract', digest,
          'out/clang-crashreports/'
      ])

      # Verify exact gsutil command
      mock_check_call.assert_any_call([
          sys.executable, process_crashreports.GSUTIL, '-q', 'cp', ANY,
          expected_dest
      ])

      # Verify files were deleted
      self.assertFalse(os.path.exists(self.crash_sh))
      self.assertFalse(os.path.exists(self.crash_c))


if __name__ == '__main__':
  unittest.main()
