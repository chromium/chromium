# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The linux specific host and platform implementation module."""

import os

import cr


class LinuxHost(cr.Host):
  """The implementation of Host for linux."""

  ACTIVE = cr.Config.From(
      GOOGLE_CODE='/usr/local/google/code',
  )

  def __init__(self):
    super(LinuxHost, self).__init__()

  def Matches(self):
    return cr.Platform.System() == 'Linux'


class LinuxPlatform(cr.Platform):
  """The implementation of Platform for the linux target."""

  ACTIVE = cr.Config.From(
      CR_BINARY=os.path.join('{CR_BUILD_DIR}', '{CR_BUILD_TARGET}'),
      CHROME_DEVEL_SANDBOX='/usr/local/sbin/chrome-devel-sandbox',
  )

  @property
  def enabled(self):
    return cr.Platform.System() == 'Linux'

  @property
  def priority(self):
    return 2
