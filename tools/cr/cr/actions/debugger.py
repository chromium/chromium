# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for the Debugger base class."""

import cr


class Debugger(cr.Action, cr.Plugin.Type):
  """Base class for implementing debuggers.

  Implementations must override the Invoke and Attach methods.
  """

  SELECTOR_ARG = '--debugger'
  SELECTOR = 'CR_DEBUGGER'
  SELECTOR_HELP = 'Sets the debugger to use for debug commands.'

  @classmethod
  def AddArguments(cls, command, parser):
    cr.Runner.AddSelectorArg(command, parser)

  @classmethod
  def ShouldInvoke(cls):
    """Checks if the debugger is attaching or launching."""
    return not cr.Runner.Skipping()

  @cr.Plugin.activemethod
  def Restart(self, targets, arguments):
    """Ask the debugger to restart.

    Defaults to a Kill Invoke sequence.
    """
    self.Kill(targets, [])
    self.Invoke(targets, arguments)

  @cr.Plugin.activemethod
  def Kill(self, targets, arguments):
    """Kill the running debugger."""
    cr.Runner.Kill(targets, arguments)

  @cr.Plugin.activemethod
  def Invoke(self, targets, arguments):
    """Invoke the program within a debugger."""
    raise NotImplementedError('Must be overridden.')

  @cr.Plugin.activemethod
  def Attach(self, targets, arguments):
    """Attach a debugger to a running program."""
    raise NotImplementedError('Must be overridden.')
