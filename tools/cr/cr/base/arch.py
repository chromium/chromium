# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for the basic architectures supported by cr."""

import cr

DEFAULT = cr.Config.From(
    CR_ENVSETUP_ARCH='{CR_ARCH}',
)


class Arch(cr.Plugin, cr.Plugin.Type):
  """Base class for implementing cr architecture targets."""

  SELECTOR = 'CR_ARCH'

  @classmethod
  def AddArguments(cls, parser):
    parser.add_argument(
        '--architecture', dest=cls.SELECTOR,
        choices=cls.Choices(),
        default=None,
        help='Sets the target architecture to use. Overrides ' + cls.SELECTOR
    )


class IA32Arch(Arch):

  ACTIVE = cr.Config.From(
      CR_ENVSETUP_ARCH='ia32',
  )


class Mips32Arch(Arch):

  ACTIVE = cr.Config.From(
      CR_ENVSETUP_ARCH='mipsel',
  )

  @property
  def enabled(self):
    return cr.AndroidPlatform.GetInstance().is_active


class X64Arch(Arch):

  ACTIVE = cr.Config.From(
      CR_ENVSETUP_ARCH='x64',
  )

  @property
  def priority(self):
    return super(X64Arch, self).priority + 1


class Arm32Arch(Arch):

  ACTIVE = cr.Config.From(
      CR_ENVSETUP_ARCH='arm',
  )

  @property
  def priority(self):
    return super(Arm32Arch, self).priority + 2

  @property
  def enabled(self):
    return cr.AndroidPlatform.GetInstance().is_active


class Arm64Arch(Arch):

  ACTIVE = cr.Config.From(
      CR_ENVSETUP_ARCH='arm64',
  )

  @property
  def enabled(self):
    return cr.AndroidPlatform.GetInstance().is_active
