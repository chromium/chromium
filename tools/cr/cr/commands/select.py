# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for the select command."""

import cr

# The set of variables SELECT writes into the client plugin to control the
# active output directory.
SELECT_OUT_VARS = ['CR_OUT_FULL']


class SelectCommand(cr.Command):
  """The implementation of the select command.

  The select command is used to set the default output directory used by all
  other commands. It does this by writing out a plugin into the client root
  that sets the active output path.
  """

  def __init__(self):
    super(SelectCommand, self).__init__()
    self.help = 'Select an output directory'
    self.description = ("""
        This makes the specified output directory the default for all future
        operations. It also invokes prepare on that directory.
        """)

  def AddArguments(self, subparsers):
    parser = super(SelectCommand, self).AddArguments(subparsers)
    self.AddPrepareArguments(parser)
    return parser

  @classmethod
  def AddPrepareArguments(cls, parser):
    parser.add_argument(
        '--no-prepare', dest='_no_prepare',
        action='store_true', default=False,
        help='Don\'t prepare the output directory.'
    )

  def Run(self):
    self.Select()

  @classmethod
  def Select(cls):
    """Performs the select.

    This is also called by the init command to auto select the new output
    directory.
    """
    cr.base.client.WriteConfig(
        use_build_dir=False,
        data=dict(CR_OUT_FULL=cr.context.Get('CR_OUT_FULL')))
    cr.base.client.PrintInfo()
    # Now we run the post select actions
    if not getattr(cr.context.args, '_no_prepare', None):
      cr.PrepareCommand.Prepare()
