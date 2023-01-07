# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for the build commands."""

import cr


class BuildCommand(cr.Command):
  """The implementation of the build command.

  This is a thin shell over the Builder.Build method of the selected builder.
  """

  def __init__(self):
    super(BuildCommand, self).__init__()
    self.help = 'Build a target'
    self.description = ("""
        Uses the specified builder for the platform to bring the target
        up to date.
        """)

  def AddArguments(self, subparsers):
    parser = super(BuildCommand, self).AddArguments(subparsers)
    cr.Builder.AddArguments(self, parser)
    cr.Target.AddArguments(self, parser, allow_multiple=True)
    self.ConsumeArgs(parser, 'the builder')
    return parser

  def Run(self):
    return cr.Builder.Build(
        cr.Target.GetTargets(), cr.context.remains)


class CleanCommand(cr.Command):
  """The implementation of the clean command.

  This is a thin shell over the Builder.Clean method of the selected builder.
  """

  def __init__(self):
    super(CleanCommand, self).__init__()
    self.help = 'Clean a target'
    self.description = (
        'Uses the specified builder to clean out built files for the target.')

  def AddArguments(self, subparsers):
    parser = super(CleanCommand, self).AddArguments(subparsers)
    cr.Builder.AddArguments(self, parser)
    cr.Target.AddArguments(self, parser, allow_multiple=True)
    self.ConsumeArgs(parser, 'the builder')
    return parser

  def Run(self):
    return cr.Builder.Clean(
        cr.Target.GetTargets(), cr.context.remains)


class RebuildCommand(cr.Command):
  """The implementation of the rebuild command.

  This is a thin shell over the Builder.Rebuild method of the selected builder.
  """

  def __init__(self):
    super(RebuildCommand, self).__init__()
    self.help = 'Rebuild a target'
    self.description = (
        'Uses the specified builder for the platform to rebuild a target.')

  def AddArguments(self, subparsers):
    parser = super(RebuildCommand, self).AddArguments(subparsers)
    cr.Builder.AddArguments(self, parser)
    cr.Target.AddArguments(self, parser, allow_multiple=True)
    self.ConsumeArgs(parser, 'the builder')
    return parser

  def Run(self):
    return cr.Builder.Rebuild(
        cr.Target.GetTargets(), cr.context.remains)
