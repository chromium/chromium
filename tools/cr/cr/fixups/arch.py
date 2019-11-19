# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for architecture output directory fixups."""

from __future__ import print_function

import cr


class _ArchInitHookHelper(cr.InitHook):
  """Base class helper for CR_ARCH value fixups."""

  def _VersionTest(self, old_version):
    _ = old_version
    return True

  def _ArchConvert(self, old_arch):
    return old_arch

  def Run(self, old_version, config):
    if old_version is None or not self._VersionTest(old_version):
      return
    old_arch = config.OVERRIDES.Find(cr.Arch.SELECTOR)
    new_arch = self._ArchConvert(old_arch)
    if new_arch != old_arch:
      print('** Fixing architecture from {0} to {1}'.format(old_arch, new_arch))
      config.OVERRIDES[cr.Arch.SELECTOR] = new_arch


class WrongArchDefaultInitHook(_ArchInitHookHelper):
  """Fixes bad initial defaults.

  In the initial versions of cr before output directories were versioned
  it was writing invalid architecture defaults. This detects that case and sets
  the architecture to the current default instead.
  """

  def _VersionTest(self, old_version):
    return old_version <= 0.0

  def _ArchConvert(self, _):
    return cr.Arch.default.name


class MipsAndArmRenameInitHook(_ArchInitHookHelper):
  """Fixes rename of Mips and Arm to Mips32 and Arm32."""

  def _ArchConvert(self, old_arch):
    if old_arch == 'mips':
      return cr.Mips32Arch.GetInstance().name
    if old_arch == 'arm':
      return cr.Arm32Arch.GetInstance().name
    return old_arch
