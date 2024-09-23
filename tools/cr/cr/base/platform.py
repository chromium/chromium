# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for the target platform support."""

from importlib import import_module
import os

import cr

DEFAULT = cr.Config.From(
    DEPOT_TOOLS=os.path.join('{GOOGLE_CODE}', 'depot_tools'),
    CHROMIUM_OUT_DIR='{CR_OUT_BASE}',)


class Platform(cr.Plugin, cr.Plugin.Type):
  """Base class for implementing cr platforms.

  A platform is the target operating system being compiled for (linux android).
  """

  _platform_module = import_module('platform', None)
  SELECTOR = 'CR_PLATFORM'

  @classmethod
  def AddArguments(cls, parser):
    parser.add_argument(
        '--platform', dest=cls.SELECTOR,
        choices=cls.Choices(),
        default=None,
        help='Sets the target platform to use. Overrides ' + cls.SELECTOR
    )

  @classmethod
  def System(cls):
    return cls._platform_module.system()

  def __init__(self):
    super(Platform, self).__init__()

  def Activate(self):
    super(Platform, self).Activate()
    if _PathFixup not in cr.context.fixup_hooks:
      cr.context.fixup_hooks.append(_PathFixup)

  @cr.Plugin.activemethod
  def Prepare(self):
    pass

  @property
  def paths(self):
    return []


def _PathFixup(base, key, value):
  """A context fixup that does platform specific modifications to the PATH."""
  if key == 'PATH':
    paths = []
    for entry in Platform.GetActivePlugin().paths:
      entry = base.Substitute(entry)
      if entry not in paths:
        paths.append(entry)
    for entry in value.split(os.path.pathsep):
      if entry not in paths:
        paths.append(entry)
    value = os.path.pathsep.join(paths)
  return value
