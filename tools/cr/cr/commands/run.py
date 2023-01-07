# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for the run command."""

import cr


class RunCommand(cr.Command):
  """The implementation of the run command.

  This first uses Builder to bring the target up to date.
  It then uses Installer to install the target (if needed), and
  finally it uses Runner to run the target.
  You can use skip version to not perform any of these steps.
  """

  def __init__(self):
    super(RunCommand, self).__init__()
    self.help = 'Invoke a target'

  def AddArguments(self, subparsers):
    parser = super(RunCommand, self).AddArguments(subparsers)
    cr.Builder.AddArguments(self, parser)
    cr.Installer.AddArguments(self, parser)
    cr.Runner.AddArguments(self, parser)
    cr.Target.AddArguments(self, parser, allow_multiple=False)
    self.ConsumeArgs(parser, 'the binary')
    return parser

  def Run(self):
    original_targets = cr.Target.GetTargets()
    targets = original_targets[:]
    for target in original_targets:
      targets.extend(target.GetRunDependencies())
    test_targets = [target for target in targets if target.is_test]
    run_targets = [target for target in targets if not target.is_test]
    if cr.Installer.Skipping():
      # No installer, only build test targets
      build_targets = test_targets
    else:
      build_targets = targets
    if build_targets:
      cr.Builder.Build(build_targets, [])
    # See if we can use restart when not installing
    if cr.Installer.Skipping():
      cr.Runner.Restart(targets, cr.context.remains)
    else:
      cr.Runner.Kill(run_targets, [])
      cr.Installer.Reinstall(run_targets, [])
      cr.Runner.Invoke(original_targets, cr.context.remains)
