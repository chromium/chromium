# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for the prepare command."""

import cr


class PrepareCommand(cr.Command):
  """The implementation of the prepare command.

  The prepare command is used to perform  the steps needed to get an output
  directory ready to use. These should not be the kind of things that need to
  happen every time you build something, but the rarer things that you re-do
  only when you get or add new source files, or change your build options.
  This delegates all it's behavior to implementations of PrepareOut. These will
  (mostly) be in the cr.actions package.
  """

  def __init__(self):
    super(PrepareCommand, self).__init__()
    self.help = 'Prepares an output directory'
    self.description = ("""
        This does any preparation needed for the output directory, such as
        running gn.
        """)

  def Run(self):
    self.Prepare()

  @classmethod
  def UpdateContext(cls):
    PrepareOut.GetActivePlugin().UpdateContext()

  @classmethod
  def Prepare(cls):
    cls.UpdateContext()
    PrepareOut.GetActivePlugin().Prepare()


class PrepareOut(cr.Plugin, cr.Plugin.Type):
  """Base class for output directory preparation plugins.

  See PrepareCommand for details.
  """

  SELECTOR = 'CR_GENERATOR'

  @classmethod
  def AddArguments(cls, parser):
    parser.add_argument(
        '--generator', dest=cls.SELECTOR,
        choices=cls.Choices(),
        default=None,
        help=('Sets the build file generator to use. ' +
              'Overrides %s.' % cls.SELECTOR)
    )

  def UpdateContext(self):
    """Update the context if needed.

    This is also used by commands that want the environment setup correctly, but
    are not going to call Prepare directly (such as sync)."""

  def Prepare(self):
    """All PrepareOut plugins must override this method to do their work."""
    raise NotImplementedError('Must be overridden.')
