# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for the Installer base class."""

import cr


class Installer(cr.Action, cr.Plugin.Type):
  """Base class for implementing installers.

  Installer implementations must implement the Uninstall and Install methods.
  If the location into which targets are built is find for running them, then
  they do not actually have to do anything.
  """

  SELECTOR_ARG = '--installer'
  SELECTOR = 'CR_INSTALLER'
  SELECTOR_HELP = 'Sets the installer to use.'

  @cr.Plugin.activemethod
  def Uninstall(self, targets, arguments):
    """Removes a target from it's installed location."""

    raise NotImplementedError('Must be overridden.')

  @cr.Plugin.activemethod
  def Install(self, targets, arguments):
    """Installs a target somewhere so that it is ready to run."""
    raise NotImplementedError('Must be overridden.')

  @cr.Plugin.activemethod
  def Reinstall(self, targets, arguments):
    """Force a target to install even if already installed.

    Default implementation is to do an Uninstall Install sequence.
    Do not call the base version if you implement a more efficient one.
    """
    self.Uninstall(targets, [])
    self.Install(targets, arguments)


class SkipInstaller(Installer):
  """An Installer the user chooses to bypass the install step of a command."""

  @property
  def priority(self):
    return super(SkipInstaller, self).priority - 1

  def Uninstall(self, targets, arguments):
    pass

  def Install(self, targets, arguments):
    pass
