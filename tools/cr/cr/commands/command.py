# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module to hold the Command plugin."""

import argparse

import cr


class Command(cr.Plugin, cr.Plugin.Type):
  """Base class for implementing cr commands.

  These are the sub-commands on the command line, and modify the
  accepted remaining arguments.
  Commands in general do not implement the functionality directly, instead they
  run a sequence of actions.
  """

  @classmethod
  def Select(cls):
    """Called to select which command is active.

    This picks a command based on the first non - argument on the command
    line.
    Returns:
      the selected command, or None if not specified on the command line.
    """
    if cr.context.args:
      return getattr(cr.context.args, '_command', None)
    return None

  def __init__(self):
    super(Command, self).__init__()
    self.help = 'Missing help: {0}'.format(self.__class__.__name__)
    self.description = None
    self.epilog = None
    self.parser = None
    self.requires_build_dir = True

  def AddArguments(self, subparsers):
    """Add arguments to the command line parser.

    Called by the main function to add the command to the command line parser.
    Commands that override this function to add more arguments must invoke
    this method.
    Args:
      subparsers: The argparse subparser manager to add this command to.
    Returns:
      the parser that was built for the command.
    """
    self.parser = subparsers.add_parser(
        self.name,
        add_help=False,
        help=self.help,
        description=self.description or self.help,
        epilog=self.epilog,
    )
    self.parser.set_defaults(_command=self)
    cr.context.AddCommonArguments(self.parser)
    cr.base.client.AddArguments(self.parser)
    return self.parser

  def ConsumeArgs(self, parser, reason):
    """Adds a remaining argument consumer to the parser.

    A helper method that commands can use to consume all remaining arguments.
    Use for things like lists of targets.
    Args:
      parser: The parser to consume remains for.
      reason: The reason to give the user in the help text.
    """
    parser.add_argument(
        '_remains', metavar='arguments',
        nargs=argparse.REMAINDER,
        help='The additional arguments to {0}.'.format(reason)
    )

  def EarlyArgProcessing(self):
    """Called to make decisions based on speculative argument parsing.

    When this method is called, enough of the command line parsing has been
    done that the command is selected. This allows the command to make any
    modifications needed before the final argument parsing is done.
    """
    cr.base.client.ApplyOutArgument()

  @cr.Plugin.activemethod
  def Run(self):
    """The main method of the command.

    This is the only thing that a command has to implement, and it should not
    call this base version.
    """
    raise NotImplementedError('Must be overridden.')
