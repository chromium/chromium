#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest
from unittest.mock import patch, MagicMock

import process_crashreports


class TestFetchRbeCrashReports(unittest.TestCase):

  @patch('os.path.getmtime')
  @patch('glob.glob')
  @patch('builtins.open', create=True)
  @patch('subprocess.check_call')
  @patch('os.path.exists')
  def test_FetchRbeCrashReports(self, mock_exists, mock_check_call, mock_open,
                                mock_glob, mock_getmtime):
    # Setup mock Siso log content
    # Format: path <tab> digest <tab> command
    digest1 = "aaa/123"
    cmd1 = (f"siso fetch -reapi_instance=test-instance -type=dir-extract "
            f"{digest1} out/clang-crashreports/")
    digest2 = "bbb/789"
    cmd2 = (f"siso fetch -reapi_instance=test-instance -type=dir-extract "
            f"{digest2} out/clang-crashreports/")

    mock_log_content = [
        "some log line\n", "auxiliary outputs:\n",
        f"out/clang-crashreports/crash1.sh\t{digest1}\t{cmd1}\n",
        "other/path\tdef/456\tsiso fetch ...\n",
        f"out/clang-crashreports/crash2.sh\t{digest2}\t{cmd2}\n"
    ]

    # Mock glob to return multiple log files
    mock_glob.return_value = ['out/Old/siso_output', 'out/Default/siso_output']

    def getmtime_side_effect(filename):
      if filename == 'out/Old/siso_output':
        return 1000
      if filename == 'out/Default/siso_output':
        return 2000
      return 0

    mock_getmtime.side_effect = getmtime_side_effect

    mock_file = MagicMock()
    mock_file.__iter__.return_value = iter(mock_log_content)
    mock_open.return_value.__enter__.return_value = mock_file
    mock_exists.return_value = True

    process_crashreports.FetchRbeCrashReports()

    mock_open.assert_called_with('out/Default/siso_output', 'r')

    expected_cmd1 = cmd1.split(' ')
    expected_cmd1[0] = process_crashreports.SISO_BINARY
    expected_cmd2 = cmd2.split(' ')
    expected_cmd2[0] = process_crashreports.SISO_BINARY
    # Verify siso fetch was called for both digests
    self.assertEqual(mock_check_call.call_count, 2)
    self.assertEqual(mock_check_call.call_args_list[0][0][0], expected_cmd1)
    self.assertEqual(mock_check_call.call_args_list[1][0][0], expected_cmd2)


if __name__ == '__main__':
  unittest.main()
