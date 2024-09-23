# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Linux Chrome OS platform."""

import os

import cr

class LinuxChromeOSPlatform(cr.Platform):
  """Platform for Linux Chrome OS target"""

  ACTIVE = cr.Config.From(
      CR_BINARY=os.path.join('{CR_BUILD_DIR}', '{CR_BUILD_TARGET}'),
      CHROME_DEVEL_SANDBOX='/usr/local/sbin/chrome-devel-sandbox',
      GN_ARG_target_os='"chromeos"',
  )

  @property
  def enabled(self):
    return cr.Platform.System() == 'Linux'

  @property
  def priority(self):
    return 2
