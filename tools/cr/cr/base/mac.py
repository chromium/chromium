# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The mac specific host and platform implementation module."""

import os

import cr


class MacHost(cr.Host):
  """The implementation of Host for mac."""

  ACTIVE = cr.Config.From(
      GOOGLE_CODE='/usr/local/google/code',
  )

  def __init__(self):
    super(MacHost, self).__init__()

  def Matches(self):
    return cr.Platform.System() == 'Darwin'


class MacPlatform(cr.Platform):
  """The implementation of Platform for the mac target."""

  ACTIVE = cr.Config.From(
      CR_BINARY=os.path.join('{CR_BUILD_DIR}', '{CR_BUILD_TARGET}'),
      CHROME_DEVEL_SANDBOX='/usr/local/sbin/chrome-devel-sandbox',
  )

  @property
  def enabled(self):
    return cr.Platform.System() == 'Darwin'

  @property
  def priority(self):
    return 2
