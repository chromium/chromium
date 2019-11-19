# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
# Note that this doesn't work with old psutil versions, as Process.name was
# changed to Process.name() at some point. This is known to work with psutil
# 5.2.2, which is the version used by vpython at the time of writing.
import psutil  # pylint: disable=import-error
import subprocess
from contrib.vr_benchmarks.desktop_runtimes import base_runtime


# pylint: disable=abstract-method
class _WMRRuntimeBase(base_runtime.DesktopRuntimeBase):
  """Base class for all WMR runtimes."""

  def GetFeatureName(self):
    return 'WindowsMixedReality'


class WMRRuntimeReal(_WMRRuntimeBase):
  """Class for using the real Windows Mixed Reality runtime for desktop tests.
  """

  WINDOWS_SYSTEM_APP_DIRECTORY = os.path.join(
      'c:', '\\', 'Windows', 'SystemApps')
  WMR_DIRECTORY_PREFIX = 'Microsoft.Windows.HolographicFirstRun'
  WMR_PROCESS_NAME = 'MixedRealityPortal.exe'
  # Arbitrary but reasonable amount of time to wait for WMR to gracefully
  # shut down when told to.
  WMR_SHUTDOWN_TIME_S = 20

  def __init__(self, *args, **kwargs):
    super(WMRRuntimeReal, self).__init__(*args, **kwargs)
    self._wmr_portal_executable = None

  def Setup(self):
    # We need to launch the WMR Portal before running any tests to ensure that
    # the runtime is ready when we try to enter VR.
    self._StartWMR()

  def WillRunStory(self):
    if not self._GetWMRProcess():
      logging.warning('WMR closed prematurely, restarting')
      self._StartWMR()

  def TearDown(self):
    wmr_process = self._GetWMRProcess()
    if wmr_process:
      try:
        wmr_process.terminate()
        wmr_process.wait(self.WMR_SHUTDOWN_TIME_S)
      except psutil.TimeoutExpired:
        logging.warning('Failed to kill WMR in %d seconds',
            self.WMR_SHUTDOWN_TIME_S)

  def GetSandboxSupported(self):
    return True

  def _StartWMR(self):
    # The WMR Portal is a UWP app, so starting it is a bit weird.
    return subprocess.Popen(['explorer.exe', self._GetWMRExecutable()])

  def _GetWMRExecutable(self):
    if self._wmr_portal_executable:
      return self._wmr_portal_executable

    for entry in os.listdir(self.WINDOWS_SYSTEM_APP_DIRECTORY):
      if entry.startswith(self.WMR_DIRECTORY_PREFIX):
        self._wmr_portal_executable = os.path.join(
            'shell:appsFolder', '%s!App' % entry)
        break

    if not self._wmr_portal_executable:
      raise RuntimeError('Unable to find WMR executable - is WMR installed?')

    return self._wmr_portal_executable

  def _GetWMRProcess(self):
    for proc in psutil.process_iter():
      if proc.name() == self.WMR_PROCESS_NAME:
        return proc
    return None


class WMRRuntimeMock(_WMRRuntimeBase):
  """Class for using the mock Windows Mixed Reality runtime for desktop tests.
  """
