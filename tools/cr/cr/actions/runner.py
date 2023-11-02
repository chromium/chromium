# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for the Runner base class."""

import cr


class Runner(cr.Action, cr.Plugin.Type):
  """Base class for implementing target runners.

  Runner implementations must implement the Kill, Run and Test methods.

  """

  SELECTOR_ARG = '--runner'
  SELECTOR = 'CR_RUNNER'
  SELECTOR_HELP = 'Sets the runner to use to execute the target.'

  @classmethod
  def AddArguments(cls, command, parser):
    parser.add_argument(
        '--test', dest='CR_TEST_TYPE',
        choices=cr.Target.TEST_TYPES,
        default=None,
        help="""
            Sets the test type to use,
            defaults to choosing based on the target.
            Set to 'no' to force it to not be a test.
            """
    )
    cls.AddSelectorArg(command, parser)

  @cr.Plugin.activemethod
  def Kill(self, targets, arguments):
    """Stops all running processes that match a target."""
    raise NotImplementedError('Must be overridden.')

  @cr.Plugin.activemethod
  def Run(self, target, arguments):
    """Run a new copy of a runnable target."""
    raise NotImplementedError('Must be overridden.')

  @cr.Plugin.activemethod
  def Test(self, target, arguments):
    """Run a test target."""
    raise NotImplementedError('Must be overridden.')

  @cr.Plugin.activemethod
  def Invoke(self, targets, arguments):
    """Invoke a target.

    This dispatches to either Test or Run depending on the target type.
    """
    for target in targets:
      if target.is_test:
        self.Test(target, arguments)
      else:
        self.Run(target, arguments)

  @cr.Plugin.activemethod
  def Restart(self, targets, arguments):
    """Force a target to restart if it is already running.

    Default implementation is to do a Kill Invoke sequence.
    Do not call the base version if you implement a more efficient one.
    """
    self.Kill(targets, [])
    self.Invoke(targets, arguments)


class SkipRunner(Runner):
  """A Runner the user chooses to bypass the run step of a command."""

  @property
  def priority(self):
    return super(SkipRunner, self).priority - 1

  def Kill(self, targets, arguments):
    pass

  def Run(self, target, arguments):
    pass

  def Test(self, target, arguments):
    pass
