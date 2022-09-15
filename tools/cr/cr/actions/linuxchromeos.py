# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Linux ChromeOS specific implementations."""

import cr


class LinuxChromeOSRunner(cr.LinuxRunner):
  """A version of cr.LinuxRunner for LinuxChromeOS.

  Running ChromeOS in Linux is the same as a normal linux Chrome build -- just
  executing the output binary.
  """

  @property
  def enabled(self):
    return cr.LinuxChromeOSPlatform.GetInstance().is_active


class LinuxChromeOSInstaller(cr.LinuxInstaller):
  """A version of cr.LinuxInstaller for LinuxChromeOS.

  This does nothing, as there is nothing to be installed.
  """

  @property
  def enabled(self):
    return cr.LinuxChromeOSPlatform.GetInstance().is_active
