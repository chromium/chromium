# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from contrib.vr_benchmarks.desktop_runtimes import base_runtime

# pylint: disable=abstract-method
class _OpenXRRuntimeBase(base_runtime.DesktopRuntimeBase):
  """Base class for all OpenXR runtimes."""

  def GetFeatureName(self):
    return 'OpenXR'


class OpenXRRuntimeReal(_OpenXRRuntimeBase):
  """Class for using the real OpenXR runtime for desktop tests.

  Currently unimplemented due to:
    * Not having access to hardware setup to test on.
    * Many OpenXR implementations are still in beta.
    * Implementations being available from multiple sources, which may all
      have different performance.
  """
# pylint: enable=abstract-method


class OpenXRRuntimeMock(_OpenXRRuntimeBase):
  """Class for using the mock OpenXR runtime for desktop tests."""

  OPENXR_CONFIG_PATH = 'XR_RUNTIME_JSON'
  # Relative to the path specified by the --mock-runtime-directory.
  OPENXR_CONFIG_PATH_VALUE = os.path.join('bin', 'openxr', 'openxr.json')

  def Setup(self):
    # All that's necessary to use the mock OpenXR runtime is to set an
    # environment variable pointing towards the mock implementation. When OpenXR
    # starts, it checks if this is set, and if so, uses the specified
    # implementation.
    # TODO(crbug.com/40619671): Switch to setting these only when the
    # browser is started once the functionality is added.
    os.environ[self.OPENXR_CONFIG_PATH] = os.path.join(
        self._mock_runtime_directory, self.OPENXR_CONFIG_PATH_VALUE)

  def WillRunStory(self):
    pass

  def TearDown(self):
    pass

  def GetSandboxSupported(self):
    return True
