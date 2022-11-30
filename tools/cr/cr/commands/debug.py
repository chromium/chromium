# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for the run command."""

import cr


class DebugCommand(cr.Command):
  """The implementation of the debug command.

  This is much like the run command except it launches the program under
  a debugger instead.
  """

  def __init__(self):
    super(DebugCommand, self).__init__()
    self.help = 'Debug a binary'

  def AddArguments(self, subparsers):
    parser = super(DebugCommand, self).AddArguments(subparsers)
    cr.Builder.AddArguments(self, parser)
    cr.Installer.AddArguments(self, parser)
    cr.Debugger.AddArguments(self, parser)
    cr.Target.AddArguments(self, parser)
    self.ConsumeArgs(parser, 'the binary')
    return parser

  def Run(self):
    targets = cr.Target.GetTargets()
    if not cr.Debugger.ShouldInvoke():
      cr.Debugger.Attach(targets, cr.context.remains)
    elif cr.Installer.Skipping():
      cr.Debugger.Restart(targets, cr.context.remains)
    else:
      cr.Builder.Build(targets, [])
      cr.Debugger.Kill(targets, [])
      cr.Installer.Reinstall(targets, [])
      cr.Debugger.Invoke(targets, cr.context.remains)
