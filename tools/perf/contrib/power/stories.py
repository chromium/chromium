# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import page_sets
import py_utils
from telemetry.internal.backends.chrome import gpu_compositing_checker
from telemetry.story import SharedState
from telemetry.story import StorySet
from telemetry.story import Story
from telemetry.page import traffic_setting
from telemetry.core import exceptions
from telemetry.internal.actions import page_action

# Wrapper classes to allow us to run existing stories as they were one single
# story. Most of the power scenarios we want to explore require long runs and we
# no not really have any existing stories that cover this. But we do have a lot
# of great short stories, that are actively maintained. Why not reuse these for
# our power runs?


class _FakeIntervalSamplingProfiler:
  @contextlib.contextmanager
  def SamplePeriod(self, period, action_runner):
    del period
    del action_runner
    yield


class _SharedState(SharedState):
  """SharedState that mimics SharedPageState but runs several stories in one go.
  """

  def __init__(self, test, finder_options, story_set, possible_browser):
    super(_SharedState, self).__init__(test, finder_options, story_set,
                                       possible_browser)

    self._current_story = None
    self._finder_options = finder_options
    self._browser = None
    self._current_tab = None
    self._extra_wpr_args = finder_options.browser_options.extra_wpr_args

    self._interval_profiling_controller = _FakeIntervalSamplingProfiler()

    self.platform.network_controller.Open(self.wpr_mode)
    self.platform.Initialize()

    self._StartBrowser()

  def _StartBrowser(self):
    assert self._browser is None
    browser_options = self._finder_options.browser_options
    self._possible_browser.SetUpEnvironment(browser_options)

    # Clear caches before starting browser.
    self.platform.FlushDnsCache()
    if browser_options.flush_os_page_caches_on_start:
      self._possible_browser.FlushOsPageCaches()

    self._browser = self._possible_browser.Create()

    if browser_options.assert_gpu_compositing:
      gpu_compositing_checker.AssertGpuCompositingEnabled(
          self._browser.GetSystemInfo())

  def _StopBrowser(self):
    if self._browser:
      self._browser.Close()
      self._browser = None
    if self._possible_browser:
      self._possible_browser.CleanUpEnvironment()

  @property
  def platform(self):
    return self._possible_browser.platform

  @property
  def current_tab(self):
    return self._current_tab

  @property
  def browser(self):
    return self._browser

  @property
  def interval_profiling_controller(self):
    return self._interval_profiling_controller

  def WillRunWrappedStory(self, story):
    archive_path = story.story_set.WprFilePathForStory(
        story, self.platform.GetOSName())
    self.platform.network_controller.StartReplay(
        archive_path, story.make_javascript_deterministic, self._extra_wpr_args)

    self.browser.Foreground()

    if self.browser.supports_tab_control:
      if len(self.browser.tabs) == 0:
        self.browser.tabs.New()
      else:
        # Close all tabs between stories
        while len(self.browser.tabs) > 1:
          self.browser.tabs[-1].Close()
        # Close the last tab and open a new one for the next story
        self.browser.tabs[-1].Close(keep_one=True)

      # Must wait for tab to commit otherwise it can commit after the next
      # navigation has begun.
      self.browser.tabs[0].WaitForDocumentReadyStateToBeComplete()

      self._current_tab = self.browser.tabs[0]

  def NavigateToPage(self, action_runner, page):
    page.RunNavigateSteps(action_runner)

  def RunPageInteractions(self, action_runner, page):
    page.RunPageInteractions(action_runner)

  def DidRunWrappedStory(self, story):
    pass

  def WillRunStory(self, story):
    self._current_story = story
    print('[  WPR     ] Downloading Archives for stories')
    story.wrapped_page_set.wpr_archive_info.DownloadArchivesIfNeeded(
        story_names=[s.name for s in story.wrapped_page_set.stories])

  def DidRunStory(self, results):
    self._current_story = None

  def CanRunStory(self, story):
    return True

  def RunStory(self, results):
    self._current_story.Run(self)

  def TearDownState(self):
    self._StopBrowser()
    self.platform.StopAllLocalServers()
    self.platform.network_controller.Close()

  def DumpStateUponStoryRunFailure(self, results):
    pass


class StoryWrapper(Story):
  def __init__(self, page_set, name):
    super(StoryWrapper, self).__init__(shared_state_class=_SharedState,
                                       name=name)

    for s in page_set.stories:
      if len(s.extra_browser_args) != 0:
        raise Exception("extra_browser_args not supported")
      if s.traffic_setting != traffic_setting.NONE:
        raise Exception("traffic_setting must be NONE")

    self._page_set = page_set

  def Run(self, shared_state):
    total = len(self._page_set.stories)
    for i, s in enumerate(self._page_set.stories):
      print('[  STORY   ] {i}/{total} {name}'.format(i=i + 1,
                                                     total=total,
                                                     name=s.name))
      try:
        shared_state.WillRunWrappedStory(s)
        s.Run(shared_state)
        shared_state.DidRunWrappedStory(s)
      except page_action.PageActionNotSupported:
        pass
      except (exceptions.TimeoutException, exceptions.LoginException,
              py_utils.TimeoutException):
        print('[  ERROR   ] {name}'.format(name=s.name))

  @property
  def wrapped_page_set(self):
    return self._page_set


class StorySetWrapper(StorySet):
  """ Wraps multiple Stories into one.

  Wraps an existing StorySet with multiple Stories into a new StorySet with just
  one Story that runs all original Stories in one go.
  """

  def __init__(self, story_set, story_name):
    super(StorySetWrapper, self).__init__()
    self._wrapped_story_set = story_set
    self.AddStory(StoryWrapper(story_set, story_name))

  @property
  def wrapped_story_set(self):
    return self._wrapped_story_set


# Helper functions to return ready to use StorySets.


def GetAllMobileSystemHealthStories():
  return StorySetWrapper(page_sets.SystemHealthStorySet(platform='mobile'),
                         'contrib_power_mobile_system_health')
