# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for the Action plugin base class."""

import cr


class Action(cr.Plugin):
  """Base class for cr actions.

  This provides the standard interface used to add actions to commands,
  including support for selecting the right implementation of an action and
  handling command line arguments for the action.
  """

  @classmethod
  def AddArguments(cls, command, parser):
    cls.AddSelectorArg(command, parser)

  @classmethod
  def AddSelectorArg(cls, command, parser):
    parser.add_argument(
        cls.SELECTOR_ARG, dest=cls.SELECTOR,
        choices=cls.Choices(),
        default=None,
        help=cls.SELECTOR_HELP + ' Overrides ' + cls.SELECTOR
    )

  @cr.Plugin.activemethod
  def Skipping(self):
    """A method that is used to detect void or skip implementations.

    Most actions have a skip version that you can select to indicate that you
    want to not perform the action at all.
    It is important that commands can detect this so they can modify the action
    sequence if there are other changes that depend on it (for instance not
    performing actions that were only there to produce the inputs of an action
    that is being skipped).

    Returns:
      True if this implementation is a skip action.
    """
    return self.name == 'skip'
