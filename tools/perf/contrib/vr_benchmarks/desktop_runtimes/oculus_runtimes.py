# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import subprocess
from contrib.vr_benchmarks.desktop_runtimes import base_runtime


# pylint: disable=abstract-method
class _BaseOculusRuntime(base_runtime.DesktopRuntimeBase):
  """Base class for all Oculus runtimes."""

  def __init__(self, *args, **kwargs):
    super(_BaseOculusRuntime, self).__init__(*args, **kwargs)

  def GetFeatureName(self):
    return 'OculusVR'


class OculusRuntimeReal(_BaseOculusRuntime):
  """Class for using the real Oculus runtime for desktop tests."""

  OCULUS_BASE_ENVIRONMENT_VARIABLE = 'OculusBase'

  def __init__(self, *args, **kwargs):
    super(OculusRuntimeReal, self).__init__(*args, **kwargs)
    self._runtime_handle = None

  def Setup(self):
    # We need to launch the Oculus client before running any tests to ensure
    # that the runtime is ready when we try to enter VR.
    self._runtime_handle = subprocess.Popen([self._GetOculusClientPath()])

  def WillRunStory(self):
    if not self._runtime_handle:
      raise RuntimeError(
          'Somehow called real Oculus pre-story without calling setup')
    if self._runtime_handle.poll() != None:
      logging.warning(
          'Oculus client closed prematurely with code %d, restarting',
          self._runtime_handle.returncode)
      self._runtime_handle = subprocess.Popen([self._GetOculusClientPath()])

  def TearDown(self):
    if not self._runtime_handle:
      raise RuntimeError(
          'Somehow called real Oculus tear down without calling setup')
    if self._runtime_handle.poll() is None:
      self._runtime_handle.terminate()

  def _GetOculusClientPath(self):
    # The install location of the Oculus runtime is set in the OculusBase
    # environment variable at install time.
    if self.OCULUS_BASE_ENVIRONMENT_VARIABLE not in os.environ:
      raise RuntimeError('Failed to find the Oculus install location. Are you '
                         'sure it\'s installed?')
    return os.path.join(os.environ[self.OCULUS_BASE_ENVIRONMENT_VARIABLE],
                        'Support', 'oculus-client', 'OculusClient.exe')


class OculusRuntimeMock(base_runtime.DesktopRuntimeBase):
  """Class for using a mock Oculus runtime for desktop tests."""
