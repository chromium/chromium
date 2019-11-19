# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module to hold adb specific action implementations."""

from __future__ import print_function

import re

import cr


class Adb(object):
  """Exposes the functionality of the adb tool to the rest of cr.

  This is intended as the only class in the cr that needs to understand the
  adb command line, and expose it in neutral form to the rest of the code.
  """

  # Tracks the set of killed target names, so we don't keep issuing kill
  # commands that are not going to have any effect.
  _kills = {}

  @classmethod
  def GetPids(cls, target):
    """Gets the set of running PIDs that match the specified target."""
    pids = []
    with target:
      output = cr.Host.Capture('{CR_ADB}', 'shell', 'ps')
    pattern = re.compile(r'\S+\s+(\d+)\s+.*{CR_PROCESS}')
    for line in output.split('\n'):
      match = re.match(pattern, line)
      if match:
        pids.append(match.group(1))
    return pids

  @classmethod
  def Run(cls, target, arguments):
    """Invoke a target binary on the device."""
    with target:
      cr.Host.Execute(
          '{CR_ADB}', 'shell', 'am', 'start',
          '-a', '{CR_ACTION}',
          '-n', '{CR_INTENT}',
          '{CR_RUN_ARGUMENTS}',
          *arguments
      )

  @classmethod
  def Kill(cls, target, _):
    """Kill all running processes for a target."""
    target_name = target.build_target
    if target_name in cls._kills:
      # already killed this target, do nothing
      return
    pids = cls.GetPids(target)
    if pids:
      with target:
        cr.Host.Execute('{CR_ADB}', 'shell', 'kill', *pids)
    elif target.verbose:
      print(target.Substitute('{CR_TARGET_NAME} not running'))
    cls._kills[target_name] = True

  @classmethod
  def Uninstall(cls, target, arguments):
    with target:
      cr.Host.Execute(
          '{CR_ADB}', 'uninstall',
          '{CR_PACKAGE}',
          *arguments
      )

  @classmethod
  def Install(cls, target, arguments):
    with target:
      cr.Host.Execute(
          '{CR_ADB}', 'install',
          '{CR_BINARY}',
          *arguments
      )

  @classmethod
  def Reinstall(cls, target, arguments):
    with target:
      cr.Host.Execute(
          '{CR_ADB}', 'install',
          '-r',
          '{CR_BINARY}',
          *arguments
      )

  @classmethod
  def AttachGdb(cls, target, arguments):
    with target:
      cr.Host.Execute(
          '{CR_ADB_GDB}',
          '--adb={CR_ADB}',
          '--symbol-dir=${CR_BUILD_DIR}/lib',
          '--program-name={CR_TARGET_NAME}',
          '--package-name={CR_PACKAGE}',
          *arguments
      )


class AdbRunner(cr.Runner):
  """An implementation of cr.Runner for the android platform."""

  @property
  def enabled(self):
    return cr.AndroidPlatform.GetInstance().is_active

  def Kill(self, targets, arguments):
    for target in targets:
      Adb.Kill(target, arguments)

  def Run(self, target, arguments):
    Adb.Run(target, arguments)

  def Test(self, target, arguments):
    with target:
      test_type = cr.context.Get('CR_TEST_TYPE')
      if test_type == cr.Target.INSTRUMENTATION_TEST:
        target_name_flag = '--test-apk'
      else:
        target_name_flag = '-s'
      cr.Host.Execute(
          '{CR_TEST_RUNNER}', test_type,
          target_name_flag, '{CR_TARGET_NAME}',
          '--{CR_TEST_MODE}',
          *arguments
      )


class AdbInstaller(cr.Installer):
  """An implementation of cr.Installer for the android platform."""

  @property
  def enabled(self):
    return cr.AndroidPlatform.GetInstance().is_active

  def Uninstall(self, targets, arguments):
    for target in targets:
      Adb.Uninstall(target, arguments)

  def Install(self, targets, arguments):
    for target in targets:
      Adb.Install(target, arguments)

  def Reinstall(self, targets, arguments):
    for target in targets:
      Adb.Reinstall(target, arguments)
