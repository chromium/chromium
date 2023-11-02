# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for the sync command."""

import os.path

import cr


class SyncCommand(cr.Command):
  """The implementation of the sync command.

  This command is a very thin shim over the gclient sync, and should remain so.
  The only significant thing it adds is that the environment is set up so that
  the run-hooks will do their work in the selected output directory.
  """

  # The configuration loaded to support this command.
  DEFAULT = cr.Config.From(
      GCLIENT_BINARY=os.path.join('{DEPOT_TOOLS}', 'gclient'),
  )

  # A placeholder for the detected gclient environment
  DETECTED = cr.Config('DETECTED')

  def __init__(self):
    super(SyncCommand, self).__init__()
    self.help = 'Sync the source tree'
    self.description = 'Run gclient sync with the right environment.'

  def AddArguments(self, subparsers):
    parser = super(SyncCommand, self).AddArguments(subparsers)
    self.ConsumeArgs(parser, 'gclient')
    # TODO(iancottrell): clean no-hooks support would be nice.
    return parser

  def Run(self):
    self.Sync(cr.context.remains)

  @staticmethod
  def Sync(args):
    cr.PrepareCommand.UpdateContext()
    # TODO(iancottrell): we should probably run the python directly,
    # rather than the shell wrapper
    # TODO(iancottrell): try to help out when the local state is not a good
    # one to do a sync in
    cr.Host.Execute('{GCLIENT_BINARY}', 'sync', *args)

  @classmethod
  def ClassInit(cls):
    # Attempt to detect gclient and it's parent repository.
    gclient_binaries = cr.Host.SearchPath('gclient')
    if gclient_binaries:
      cls.DETECTED.Set(GCLIENT_BINARY=gclient_binaries[0])
      cls.DETECTED.Set(DEPOT_TOOLS=os.path.dirname(gclient_binaries[0]))
