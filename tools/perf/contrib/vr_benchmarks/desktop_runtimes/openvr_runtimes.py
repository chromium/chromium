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
# We expect this to be missing on all non-Windows platforms.
try:
  import _winreg
except ImportError:
  pass

from contrib.vr_benchmarks.desktop_runtimes import base_runtime


# pylint: disable=abstract-method
class _OpenVRRuntimeBase(base_runtime.DesktopRuntimeBase):
  """Base class for all OpenVR runtimes."""

  def GetFeatureName(self):
    return 'OpenVR'


class OpenVRRuntimeReal(_OpenVRRuntimeBase):
  """Class for using the real OpenVR runtime for desktop tests.

  In order for this to work properly, room setup must have been completed,
  otherwise OpenVR won't properly show/render content. Unfortunately, there's no
  way to tell whether that's been done or not, so we can't enforce it during
  setup.
  """

  STEAM_REGISTRY_PATH = 'Software\\Valve\\Steam'
  STEAM_EXE_REGISTRY_KEY = 'SteamExe'
  STEAM_VR_APP_ID = 250820
  STEAM_VR_PROCESS_NAME = 'vrmonitor.exe'
  # Arbitrary but reasonable amount of time to wait for SteamVR to gracefully
  # shut down when told to.
  STEAM_VR_SHUTDOWN_TIME_S = 20

  def __init__(self, *args, **kwargs):
    super(OpenVRRuntimeReal, self).__init__(*args, **kwargs)

    # Check if Steam is installed for all users, and if not, for the current
    # user.
    self._steam_path = self._GetSteamExePath(_winreg.HKEY_LOCAL_MACHINE)
    if not self._steam_path:
      self._steam_path = self._GetSteamExePath(_winreg.HKEY_CURRENT_USER)
    if not self._steam_path:
      raise RuntimeError(
          'Unable to retrieve Steam install location - are you sure it\'s '
          'installed?')

  def Setup(self):
    self._StartSteamVr()

  def WillRunStory(self):
    steam_vr_process = self._GetSteamVrProcess()
    if not steam_vr_process:
      logging.warning('SteamVR closed prematurely, restarting')
      self._StartSteamVr()

  def TearDown(self):
    steam_vr_process = self._GetSteamVrProcess()
    if steam_vr_process:
      try:
        steam_vr_process.terminate()
        steam_vr_process.wait(self.STEAM_VR_SHUTDOWN_TIME_S)
      except psutil.TimeoutExpired:
        logging.warning('Failed to kill SteamVR in %d seconds',
            self.STEAM_VR_SHUTDOWN_TIME_S)

  def _GetSteamExePath(self, hkey):
    try:
      registry_key = _winreg.OpenKey(hkey, self.STEAM_REGISTRY_PATH)
      value, _ = _winreg.QueryValueEx(registry_key, self.STEAM_EXE_REGISTRY_KEY)
      return value
    except OSError:
      return None

  def _StartSteamVr(self):
     # Launch Steam (if it's not already open) and launch SteamVR through it.
    subprocess.call([self._steam_path, '-applaunch', str(self.STEAM_VR_APP_ID)])

  def _GetSteamVrProcess(self):
    for proc in psutil.process_iter():
      if proc.name() == self.STEAM_VR_PROCESS_NAME:
        return proc
    return None


class OpenVRRuntimeMock(_OpenVRRuntimeBase):
  """Class for using the mock OpenVR runtime for desktop tests."""

  OPENVR_OVERRIDE_ENV_VAR = "VR_OVERRIDE"
  OPENVR_CONFIG_PATH = "VR_CONFIG_PATH"
  OPENVR_LOG_PATH = "VR_LOG_PATH"

  def __init__(self, *args, **kwargs):
    super(OpenVRRuntimeMock, self).__init__(*args, **kwargs)

    if self._finder_options.mock_runtime_directory:
      self._mock_runtime_directory = os.path.abspath(
          self._finder_options.mock_runtime_directory)
    else:
      self._mock_runtime_directory = os.path.abspath(os.path.join(
          self._possible_browser.browser_directory, 'mock_vr_clients'))
      logging.warning('Using mock directory %s', self._mock_runtime_directory)

  def Setup(self):
    # All that's necessary to use the mock OpenVR runtime is to set a few
    # environment variables pointing towards the mock implementation. When
    # OpenVR starts, it checks if these are set, and if so, uses the
    # implementation that they specify instead of loading the real one.
    # TODO(https://crbug.com/944890): Switch to setting these only when the
    # browser is started once the functionality is added.
    os.environ[self.OPENVR_OVERRIDE_ENV_VAR] = self._mock_runtime_directory
    # We don't really care about what these are set to, but they need to be set
    # in order for the mock to work.
    os.environ[self.OPENVR_CONFIG_PATH] = os.getcwd()
    os.environ[self.OPENVR_LOG_PATH] = os.getcwd()

  def WillRunStory(self):
    pass

  def TearDown(self):
    # os.environ is limited to this Python process and its subprocesses, so
    # we don't need to clean up anything.
    pass

  def GetSandboxSupported(self):
    return True
