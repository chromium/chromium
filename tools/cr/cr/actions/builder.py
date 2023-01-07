# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for the Builder base class."""

import difflib

import cr


class Builder(cr.Action, cr.Plugin.Type):
  """Base class for implementing builders.

  Builder implementations must override the Build and Clean methods at a
  minimum to build a target and clean up back to a pristine state respectively.
  They can also override Rebuild if they are able to handle it in a more
  efficient way that a Clean Build sequence.
  They should override the GetTargets method to return the set of valid targets
  the build system knows about, and override IsTarget if they can implement it
  more efficiently than checking from presents in the result of GetTargets.
  """

  SELECTOR_ARG = '--builder'
  SELECTOR = 'CR_BUILDER'
  SELECTOR_HELP = 'Sets the builder to use to update dependencies.'

  @cr.Plugin.activemethod
  def Build(self, targets, arguments):
    raise NotImplementedError('Must be overridden.')

  @cr.Plugin.activemethod
  def Clean(self, targets, arguments):
    """Clean temporary files built by a target."""
    raise NotImplementedError('Must be overridden.')

  @cr.Plugin.activemethod
  def Rebuild(self, targets, arguments):
    """Make a target build even if it is up to date.

    Default implementation is to do a Clean and Build sequence.
    Do not call the base version if you implement a more efficient one.
    """
    self.Clean(targets, [])
    self.Build(targets, arguments)

  @cr.Plugin.activemethod
  def GetTargets(self):
    """Gets the full set of targets supported by this builder.

    Used in automatic target name transformations, and also in offering the
    user choices.
    """
    return []

  @cr.Plugin.activemethod
  def IsTarget(self, target_name):
    """Check if a target name is on the builder knows about."""
    return target_name in self.GetTargets()

  @cr.Plugin.activemethod
  def GuessTargets(self, target_name):
    """Returns a list of closest matching targets for a named target."""
    return difflib.get_close_matches(target_name, self.GetTargets(), 10, 0.4)


class SkipBuilder(Builder):
  """The "skip" version of a Builder, causes the build step to be skipped."""

  @property
  def priority(self):
    return super(SkipBuilder, self).priority - 1

  def Build(self, targets, arguments):
    pass

  def Clean(self, targets, arguments):
    pass

  def IsTarget(self, target_name):
    return True
