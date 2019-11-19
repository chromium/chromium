# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys


ALL_RUNTIMES = frozenset(['OculusVR', 'OpenVR', 'WindowsMixedReality'])
SANDBOX_FEATURE = 'XRSandbox'


class DesktopRuntimeBase(object):
  """Interface for all desktop VR runtimes."""

  def __init__(self, finder_options, possible_browser):
    self._finder_options = finder_options
    self._possible_browser = possible_browser

    self._finder_options.browser_options.AppendExtraBrowserArgs(
        '--enable-features=%s' % self.GetFeatureName())
    # Disable all other runtimes other than the current one.
    for feature in ALL_RUNTIMES - set([self.GetFeatureName()]):
      self._finder_options.browser_options.AppendExtraBrowserArgs(
        '--disable-features=%s' % feature)

    if self.GetSandboxSupported():
      self._finder_options.browser_options.AppendExtraBrowserArgs(
        '--enable-features=%s' % SANDBOX_FEATURE)
      self.SetupSandboxAcls(os.path.abspath(possible_browser.browser_directory))
    else:
      self._finder_options.browser_options.AppendExtraBrowserArgs(
        '--disable-features=%s' % SANDBOX_FEATURE)

  def Setup(self):
    """Called once before any stories are run."""
    raise NotImplementedError(
        'No runtime setup defined for %s' % self.__class__.__name__)

  def WillRunStory(self):
    """Called before each story is run."""
    raise NotImplementedError(
        'No runtime pre-story defined for %s' % self.__class__.__name__)

  def TearDown(self):
    """Called once after all stories are run."""
    raise NotImplementedError(
        'No runtime tear down defined for %s' % self.__class__.__name__)

  def GetFeatureName(self):
    raise NotImplementedError(
        'No feature defined for %s' % self.__class__.__name__)

  def GetSandboxSupported(self):
    # The majority of runtimes don't support being run in the sandboxed process
    # yet, so default to disabling it.
    return False

  def SetupSandboxAcls(self, directory):
    """Sets up ACLs on a directory for sandbox support.

    No-op of the ACLs are already set.

    Args:
      directory: A path pointing to the directory to set the ACLs on.
    """
    # Import here instead of at the top of the file to prevent issues with
    # Telemetry unittests not properly including all dependencies for stuff
    # it's testing.
    # //chrome/browser/vr/test/
    import_dir = os.path.join(
        os.path.dirname(__file__), '..', '..', '..', '..', '..',
        'chrome', 'browser', 'vr', 'test')
    if import_dir not in sys.path:
      sys.path.append(import_dir)
    # pylint: disable=import-error, wrong-import-position
    import run_xr_browser_tests
    run_xr_browser_tests.SetupWindowsACLs(directory)
