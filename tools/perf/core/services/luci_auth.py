# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import subprocess
import sys


_RE_INFO_USER_EMAIL = r'Logged in as (?P<email>\S+)\.$'


class AuthorizationError(Exception):
  pass


def _RunCommand(command):
  try:
    return subprocess.check_output(
        ['luci-auth', command], stderr=subprocess.STDOUT,
        universal_newlines=True)
  except subprocess.CalledProcessError as exc:
    raise AuthorizationError(exc.output.strip())


def CheckLoggedIn():
  """Check that the user is currently logged in.

  Otherwise sys.exit immediately with the error message from luci-auth
  instructing the user how to log in.
  """
  try:
    GetAccessToken()
  except AuthorizationError as exc:
    sys.exit(exc.message)


def GetAccessToken():
  """Get an access token to make requests on behalf of the logged in user."""
  return _RunCommand('token').rstrip()


def GetUserEmail():
  """Get the email address of the currently logged in user."""
  output = _RunCommand('info')
  m = re.match(_RE_INFO_USER_EMAIL, output, re.MULTILINE)
  assert m, 'Failed to parse luci-auth info output.'
  return m.group('email')
