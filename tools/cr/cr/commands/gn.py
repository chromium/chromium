# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for the gn command."""

import os

import cr


class GnCommand(cr.Command):
  """The implementation of the gn command.

  The gn command is meant for running the gn tool without having to manually
  specify an out directory.
  """

  def __init__(self):
    super(GnCommand, self).__init__()
    self.help = 'Run gn with the currently selected out directory'
    self.description = ("""
        Runs the gn command with the currently selected out directory as the
        second argument.
        """)

  def AddArguments(self, subparsers):
    parser = super(GnCommand, self).AddArguments(subparsers)
    self.ConsumeArgs(parser, 'gn')
    return parser

  def Run(self):
    out_path = os.path.join(cr.context['CR_SRC'],
                            cr.context['CR_OUT_FULL'])
    args = cr.context.remains
    if args:
      cr.Host.Execute('gn', args[0], out_path, *args[1:])
    else:
      cr.Host.Execute('gn')
