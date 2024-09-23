# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets.login_helpers import google_login
from page_sets.system_health import platforms
from page_sets.system_health import story_tags
from page_sets.system_health import system_health_story


IDLE_TIME_IN_SECONDS = 100
SAMPLING_INTERVAL_IN_SECONDS = 1
STEPS = IDLE_TIME_IN_SECONDS // SAMPLING_INTERVAL_IN_SECONDS


class _LongRunningStory(system_health_story.SystemHealthStory):
  """Abstract base class for long running stories."""
  ABSTRACT_STORY = True
  BACKGROUND = False

  def RunPageInteractions(self, action_runner):
    super(_LongRunningStory, self).RunPageInteractions(action_runner)
    if self.BACKGROUND:
      action_runner.tab.browser.tabs.New()
    if self._take_memory_measurement:
      action_runner.MeasureMemory()
    for _ in range(STEPS):
      action_runner.Wait(SAMPLING_INTERVAL_IN_SECONDS)
      if self._take_memory_measurement:
        action_runner.MeasureMemory()

  @classmethod
  def GenerateStoryDescription(cls):
    if cls.BACKGROUND:
      return ('Load %s then open a new blank tab and let the loaded page stay '
              'in background for %s seconds.' % (cls.URL, IDLE_TIME_IN_SECONDS))
    return ('Load %s then let it stay in foreground for %s seconds.' %
            (cls.URL, IDLE_TIME_IN_SECONDS))

  def WillStartTracing(self, chrome_trace_config):
    # Long running stories generate large traces, so use a large tracing buffer
    # size.
    chrome_trace_config.SetTraceBufferSizeInKb(350 * 1024)



##############################################################################
# Long running Gmail stories.
##############################################################################

# TODO(rnephew): Merge _Login() and _DidLoadDocument() with methods in
# loading_stories.
class _LongRunningGmailBase(_LongRunningStory):
  URL = 'https://mail.google.com/mail/'
  ABSTRACT_STORY = True

  def _Login(self, action_runner):
    google_login.LoginGoogleAccount(action_runner, 'googletest')

    # Navigating to https://mail.google.com immediately leads to an infinite
    # redirection loop due to a bug in WPR (see
    # https://github.com/chromium/web-page-replay/issues/70). We therefore first
    # navigate to a sub-URL to set up the session and hit the resulting
    # redirection loop. Afterwards, we can safely navigate to
    # https://mail.google.com.
    action_runner.Navigate(
        'https://mail.google.com/mail/mu/mp/872/trigger_redirection_loop')
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()

class _LongRunningGmailMobileBase(_LongRunningGmailBase):
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  # TODO(crbug.com/40584277): Story breaks if login is skipped during replay.
  SKIP_LOGIN = False

  def _DidLoadDocument(self, action_runner):
    # Close the "Get Inbox by Gmail" interstitial.
    action_runner.WaitForJavaScriptCondition(
        'document.querySelector("#isppromo a") !== null')
    action_runner.ExecuteJavaScript(
        'document.querySelector("#isppromo a").click()')
    # Wait until the UI loads.
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("apploadingdiv").style.height === "0px"')


class _LongRunningGmailDesktopBase(_LongRunningGmailBase):
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY

  def _DidLoadDocument(self, action_runner):
    # Wait until the UI loads.
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("loading").style.display === "none"')


class LongRunningGmailMobileForegroundStory(_LongRunningGmailMobileBase):
  NAME = 'long_running:tools:gmail-foreground'
  TAGS = [story_tags.YEAR_2016]


class LongRunningGmailDesktopForegroundStory(_LongRunningGmailDesktopBase):
  NAME = 'long_running:tools:gmail-foreground'
  TAGS = [story_tags.HEALTH_CHECK, story_tags.YEAR_2016]


class LongRunningGmailMobileBackgroundStory(_LongRunningGmailMobileBase):
  BACKGROUND = True
  NAME = 'long_running:tools:gmail-background'
  TAGS = [story_tags.YEAR_2016]
  # This runs a gmail story in a background tab, and tabs aren't supported
  # on WebView.
  WEBVIEW_NOT_SUPPORTED = True


class LongRunningGmailDesktopBackgroundStory(_LongRunningGmailDesktopBase):
  BACKGROUND = True
  NAME = 'long_running:tools:gmail-background'
  TAGS = [story_tags.YEAR_2016]
