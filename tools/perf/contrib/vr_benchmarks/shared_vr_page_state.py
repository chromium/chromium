# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import path_util
path_util.AddAndroidPylibToPath()
from devil.android.sdk import keyevent  # pylint: disable=import-error
from telemetry.page import shared_page_state
from contrib.vr_benchmarks.desktop_runtimes import openxr_runtimes

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

    if self.platform.GetOSName().lower() == 'android':
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

  @property
  def recording_wpr(self):
    return self._finder_options.recording_wpr

  def ShouldNavigateToBlankPageBeforeFinishing(self):
    # TODO(crbug.com/41446778): Always navigate once the issue with
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
    self._orig_doff_screen_timeout_ms = None

  def WillRunStory(self, story):
    super(AndroidSharedVrPageState, self).WillRunStory(story)
    if self.platform.android_action_runner.IsXrDevice():
      # XR devices may keep screen off when not weared.
      self._orig_doff_screen_timeout_ms = self.platform.android_action_runner.RunCommand(
          'settings get system doff_screen_timeout_ms').strip()
      # The value should never be empty, and we don't know how to restore the
      # empty value.
      assert self._orig_doff_screen_timeout_ms
      self.platform.android_action_runner.RunCommand(
          'settings put system doff_screen_timeout_ms 0')
      self.platform.android_action_runner.InputKeyEvent(keyevent.KEYCODE_WAKEUP)
    self.platform.android_action_runner.TurnScreenOn()

  def DidRunStory(self, results):
    super(AndroidSharedVrPageState, self).DidRunStory(results)
    if self.platform.android_action_runner.IsXrDevice():
      if self._orig_doff_screen_timeout_ms == 'null':
        # This means the setting wasn't set.
        cmd = 'settings delete system doff_screen_timeout_ms'
      elif self._orig_doff_screen_timeout_ms:
        cmd = f'settings put system doff_screen_timeout_ms {self._orig_doff_screen_timeout_ms}'
      self.platform.android_action_runner.RunCommand(cmd)

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
      'openxr': {
          MOCK_RUNTIME: openxr_runtimes.OpenXRRuntimeMock,
          REAL_RUNTIME: openxr_runtimes.OpenXRRuntimeReal,
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

  def WillRunStory(self, story):
    super(WindowsSharedVrPageState, self).WillRunStory(story)
    self._desktop_runtime.WillRunStory()

  def TearDownState(self):
    super(WindowsSharedVrPageState, self).TearDownState()
    self._desktop_runtime.TearDown()
