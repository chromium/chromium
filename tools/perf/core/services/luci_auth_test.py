# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess
import unittest
from unittest import mock

from core.services import luci_auth


class TestLuciAuth(unittest.TestCase):
  def setUp(self):
    self.check_output = mock.patch('subprocess.check_output').start()

  def _MockSubprocessOutput(self, output, return_code=0):
    if not return_code:
      self.check_output.return_value = output
    else:
      def SideEffect(cmd, *args, **kwargs):
        del args  # Unused.
        del kwargs  # Unused.
        raise subprocess.CalledProcessError(return_code, cmd, output=output)
      self.check_output.side_effect = SideEffect

  def tearDown(self):
    mock.patch.stopall()

  @mock.patch('sys.exit')
  def testCheckLoggedIn_success(self, sys_exit):
    self._MockSubprocessOutput('access-token')
    self.check_output.return_value = 'access-token'
    luci_auth.CheckLoggedIn()
    self.assertFalse(sys_exit.mock_calls)

  @mock.patch('sys.exit')
  def testCheckLoggedIn_failure(self, sys_exit):
    self._MockSubprocessOutput('Not logged in.', return_code=1)
    luci_auth.CheckLoggedIn()
    sys_exit.assert_called_once_with('Not logged in.')

  def testGetAccessToken_success(self):
    self._MockSubprocessOutput('access-token')
    self.assertEqual(luci_auth.GetAccessToken(), 'access-token')

  def testGetAccessToken_failure(self):
    self._MockSubprocessOutput('Not logged in.', return_code=1)
    with self.assertRaises(luci_auth.AuthorizationError):
      luci_auth.GetAccessToken()

  def testGetUserEmail(self):
    self._MockSubprocessOutput(
        'Logged in as someone@example.com.\n'
        'OAuth token details:\n'
        '  Client ID: abcd1234foo.bar.example.com\n'
        '  Scopes:\n'
        '    https://www.example.com/auth/userinfo.email\n')
    self.assertEqual(luci_auth.GetUserEmail(), 'someone@example.com')
