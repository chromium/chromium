# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import cr


class GdbDebugger(cr.Debugger):
  """An implementation of cr.Debugger that launches gdb."""

  DETECTED = cr.Config('DETECTED')

  @property
  def enabled(self):
    return (cr.LinuxPlatform.GetInstance().is_active and
            self.DETECTED.Find('CR_GDB'))

  def Invoke(self, targets, arguments):
    for target in targets:
      with target:
        cr.Host.Execute(
            '{CR_GDB}', '--eval-command=run', '--args',
            '{CR_BINARY}',
            '{CR_RUN_ARGUMENTS}',
            *arguments
      )

  def Attach(self, targets, arguments):
    raise NotImplementedError('Attach not currently supported for gdb.')

  @classmethod
  def ClassInit(cls):
    # Attempt to find a valid gdb on the path.
    gdb_binaries = cr.Host.SearchPath('gdb')
    if gdb_binaries:
      cls.DETECTED.Set(CR_GDB=gdb_binaries[0])
