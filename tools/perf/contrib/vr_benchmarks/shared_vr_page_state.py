# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import time
from core import path_util
path_util.AddAndroidPylibToPath()
from pylib.utils import shared_preference_utils
from telemetry.core import android_platform
from telemetry.core import util
from telemetry.page import shared_page_state
from contrib.vr_benchmarks.desktop_runtimes import oculus_runtimes
from contrib.vr_benchmarks.desktop_runtimes import openvr_runtimes
from contrib.vr_benchmarks.desktop_runtimes import wmr_runtimes


CARDBOARD_PATH = os.path.join('chrome', 'android', 'shared_preference_files',
                              'test', 'vr_cardboard_skipdon_setupcomplete.json')
WEBXR_CONSENT_DIALOG_DISABLE_FLAG = (
    '--disable-xr-device-consent-prompt-for-testing')


class SharedVrPageStateFactory(shared_page_state.SharedPageState):
  """"Factory" for picking the correct SharedVrPageState subclass.

  This is a hacky way to automatically change the shared page state that's used
  depending on which platform the benchmark is being run on. The
  shared_page_state_class that gets passed to the Page constructor must be an
  instance of SharedState, so we can't just pass a function pointer that returns
  an instance of the correct subclass when called.

  Additionally, we only really know what platform we're being run on after
  SharedPageState's constructor is called, as we can't rely on the given
  possible_browser to determine it.

  So, we have to call SharedPageState's constructor, find out which platform
  we're being run on, switch our class out for the correct one, and
  re-construct ourselves.
  """
  def __init__(self, test, finder_options, story_set, possible_browser=None):
    super(SharedVrPageStateFactory, self).__init__(
        test, finder_options, story_set, possible_browser)

    if isinstance(self.platform, android_platform.AndroidPlatform):
      self.__class__ = AndroidSharedVrPageState
    elif self.platform.GetOSName().lower() == 'win':
      self.__class__ = WindowsSharedVrPageState
    else:
      raise NotImplementedError(
          'No VR SharedPageState implemented for platform %s' %
          self.platform.GetOSName())
    # Use self._possible_browser to avoid duplicate computation if
    # possible_browser is None.
    self.__init__(test, finder_options, story_set, self._possible_browser)


class _SharedVrPageState(shared_page_state.SharedPageState):
  """Abstract, platform-independent SharedPageState for VR tests.

  Must be subclassed for each platform, since VR setup and tear down differs
  between each.
  """
  def __init__(self, test, finder_options, story_set, possible_browser=None):
    super(_SharedVrPageState, self).__init__(
        test, finder_options, story_set, possible_browser)
    self._story_set = story_set
    self._finder_options.AppendExtraBrowserArgs(
        [WEBXR_CONSENT_DIALOG_DISABLE_FLAG])

  @property
  def recording_wpr(self):
    return self._finder_options.recording_wpr

  def ShouldNavigateToBlankPageBeforeFinishing(self):
    # TODO(https://crbug.com/941715): Always navigate once the issue with
    # tracing metadata for the XR device process not being present when
    # navigation occurs is fixed.
    return False


class AndroidSharedVrPageState(_SharedVrPageState):
  """Android-specific VR SharedPageState.

  Platform-specific functionality:
  1. Performs Android VR-specific setup such as installing and configuring
     additional APKs that are necessary for testing.
  2. Cycles the screen off then on before each story, similar to how
     AndroidScreenRestorationSharedState ensures that the screen is on. See
     _CycleScreen() for an explanation on the reasoning behind this.
  """
  def __init__(self, test, finder_options, story_set, possible_browser=None):
    super(AndroidSharedVrPageState, self).__init__(
        test, finder_options, story_set, possible_browser)
    if self._finder_options.remove_system_vrcore:
      self._RemoveSystemVrCore()
    if not self._finder_options.disable_vrcore_install:
      self._InstallVrCore()
    self._ConfigureVrCore(os.path.join(path_util.GetChromiumSrcDir(),
                                       self._finder_options.shared_prefs_file))
    self._InstallNfcApk()
    if not self._finder_options.disable_keyboard_install:
      self._InstallKeyboardApk()

  def _RemoveSystemVrCore(self):
    # Import done here since importing Devil on Windows breaks the Telemetry
    # unittests.
    from devil.android import forwarder # pylint: disable=import-error
    # Close the existing network controller since RemoveSystemPackages could
    # potentially reboot the device, which breaks the existing port forwarding
    # and makes it impossible to cleanly re-establish it if the network
    # controller is still running.
    self.platform.network_controller.Close()
    self.platform.RemoveSystemPackages(['com.google.vr.vrcore'])
    # Re-open the network controller, which in turn re-establishes the adb
    # forwarding necessary for the local server to work. Since port forwarding
    # often refuses to work for a short period after rebooting, try several
    # times.
    for _ in xrange(5):
      try:
        self.platform.network_controller.Open(self.wpr_mode)
        break
      except forwarder.HostForwarderError:
        logging.error(
            'Failed to open network controller, will retry after a short nap')
        time.sleep(5)

  def _InstallVrCore(self):
    """Installs the VrCore APK."""
    self.platform.InstallApplication(
        os.path.join(path_util.GetChromiumSrcDir(), 'third_party',
                     'gvr-android-sdk', 'test-apks', 'vr_services',
                     'vr_services_current.apk'))

  def _ConfigureVrCore(self, filepath):
    """Configures VrCore using the provided settings file."""
    settings = shared_preference_utils.ExtractSettingsFromJson(filepath)
    for setting in settings:
      shared_pref = self.platform.GetSharedPrefs(
          setting['package'], setting['filename'],
          use_encrypted_path=setting.get('supports_encrypted_path', False))
      shared_preference_utils.ApplySharedPreferenceSetting(
          shared_pref, setting)

  def _InstallNfcApk(self):
    """Installs the APK that allows VR tests to simulate a headset NFC scan."""
    chromium_root = path_util.GetChromiumSrcDir()
    # Find the most recently build APK
    candidate_apks = []
    for build_path in util.GetBuildDirectories(chromium_root):
      apk_path = os.path.join(build_path, 'apks', 'VrNfcSimulator.apk')
      if os.path.exists(apk_path):
        last_changed = os.path.getmtime(apk_path)
        candidate_apks.append((last_changed, apk_path))

    if not candidate_apks:
      raise RuntimeError(
          'Could not find VrNfcSimulator.apk in a build output directory')
    newest_apk_path = sorted(candidate_apks)[-1][1]
    self.platform.InstallApplication(
        os.path.join(chromium_root, newest_apk_path))

  def _InstallKeyboardApk(self):
    """Installs the VR Keyboard APK."""
    self.platform.InstallApplication(
        os.path.join(path_util.GetChromiumSrcDir(), 'third_party',
                     'gvr-android-sdk', 'test-apks', 'vr_keyboard',
                     'vr_keyboard_current.apk'))

  def WillRunStory(self, page):
    super(AndroidSharedVrPageState, self).WillRunStory(page)
    if not self._finder_options.disable_screen_reset:
      self._CycleScreen()

  def TearDownState(self):
    super(AndroidSharedVrPageState, self).TearDownState()
    # Re-apply Cardboard as the viewer to leave the device in a consistent
    # state after a benchmark run
    # TODO(bsheedy): Remove this after crbug.com/772969 is fixed
    self._ConfigureVrCore(os.path.join(path_util.GetChromiumSrcDir(),
                                       CARDBOARD_PATH))

  def _CycleScreen(self):
    """Cycles the screen off then on.

    This is because VR test devices are set to have normal screen brightness and
    automatically turn off after several minutes instead of the usual approach
    of having the screen always on at minimum brightness. This is due to the
    motion-to-photon latency test being sensitive to screen brightness, and min
    brightness does not work well for it.

    Simply using TurnScreenOn does not actually reset the timer for turning off
    the screen, so instead cycle the screen to refresh it periodically.
    """
    self.platform.android_action_runner.TurnScreenOff()
    self.platform.android_action_runner.TurnScreenOn()

  def ShouldNavigateToBlankPageBeforeFinishing(self):
    # Android devices generate a lot of heat while in VR, so navigate away from
    # the VR page after we're done collecting data so that we aren't in VR while
    # metric calculation is occurring.
    return True


class WindowsSharedVrPageState(_SharedVrPageState):
  """Windows-specific VR SharedPageState.

  Platform-specific functionality involves starting and stopping different
  VR runtimes before and after all stories are run.
  """

  # Constants to make the below map more readable
  MOCK_RUNTIME = False
  REAL_RUNTIME = True
  # Map of runtime names to runtime classes for both real and mock
  # implementations. Real runtimes require specialized hardware and software
  # to be installed, i.e. exactly how a real user would use VR. Mock runtimes
  # avoid this, but can't necessarily be implemented.
  DESKTOP_RUNTIMES = {
    'oculus': {
      MOCK_RUNTIME: oculus_runtimes.OculusRuntimeMock,
      REAL_RUNTIME: oculus_runtimes.OculusRuntimeReal,
    },
    'openvr': {
      MOCK_RUNTIME: openvr_runtimes.OpenVRRuntimeMock,
      REAL_RUNTIME: openvr_runtimes.OpenVRRuntimeReal
    },
    'wmr': {
      MOCK_RUNTIME: wmr_runtimes.WMRRuntimeMock,
      REAL_RUNTIME: wmr_runtimes.WMRRuntimeReal,
    },
  }

  def __init__(self, test, finder_options, story_set, possible_browser):
    super(WindowsSharedVrPageState, self).__init__(
        test, finder_options, story_set, possible_browser)

    # Get the specific runtime implementation depending on runtime choice and
    # whether we're using the real or mock one.
    self._desktop_runtime = self.DESKTOP_RUNTIMES[
        self._finder_options.desktop_runtime][
            self._finder_options.use_real_runtime](
                self._finder_options, self._possible_browser)
    self._desktop_runtime.Setup()

  def WillRunStory(self, page):
    super(WindowsSharedVrPageState, self).WillRunStory(page)
    self._desktop_runtime.WillRunStory()

  def TearDownState(self):
    super(WindowsSharedVrPageState, self).TearDownState()
    self._desktop_runtime.TearDown()
