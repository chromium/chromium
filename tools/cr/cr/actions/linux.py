# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module to hold linux specific action implementations."""

import cr


class LinuxRunner(cr.Runner):
  """An implementation of cr.Runner for the linux platform.

  This supports directly executing the binaries from the output directory.
  """

  @property
  def enabled(self):
    return cr.LinuxPlatform.GetInstance().is_active

  def Kill(self, targets, arguments):
    # Not needed on Linux because the target generally runs in the same shell
    # and can be killed using Ctrl-C.
    pass

  def Run(self, target, arguments):
    with target:
      cr.Host.Execute('{CR_BINARY}', '{CR_RUN_ARGUMENTS}', *arguments)

  def Test(self, target, arguments):
    self.Run(target, arguments)


class LinuxInstaller(cr.Installer):
  """An implementation of cr.Installer for the linux platform.

  This does nothing, the linux runner works from the output directory, there
  is no need to install anywhere.
  """

  @property
  def enabled(self):
    return cr.LinuxPlatform.GetInstance().is_active

  def Uninstall(self, targets, arguments):
    pass

  def Install(self, targets, arguments):
    pass

  def Reinstall(self, targets, arguments):
    pass
