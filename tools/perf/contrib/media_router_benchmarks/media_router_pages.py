# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import story
from telemetry.page import shared_page_state
from telemetry.util import js_template

from contrib.media_router_benchmarks.media_router_base_page import MediaRouterBasePage


SESSION_WAIT_TIME = 300  # 5 minutes
WAIT_TIME_SEC = 10

class SharedState(shared_page_state.SharedPageState):
  """Shared state that restarts the browser for every single story."""

  def __init__(self, test, finder_options, story_set, possible_browser):
    super(SharedState, self).__init__(
        test, finder_options, story_set, possible_browser)

  def DidRunStory(self, results):
    super(SharedState, self).DidRunStory(results)
    self._StopBrowser()


class CastIdlePage(MediaRouterBasePage):
  """Cast page to open a cast-enabled page and do nothing."""

  def __init__(self, page_set, name='basic_test.html'):
    super(CastIdlePage, self).__init__(
        page_set=page_set,
        url='file://test_site/basic_test.html',
        shared_page_state_class=SharedState,
        name=name)

  def RunPageInteractions(self, action_runner):
    # Wait for 5s after Chrome is opened in order to get consistent results.
    action_runner.Wait(5)
    with action_runner.CreateInteraction('Idle'):
      action_runner.ExecuteJavaScript('collectPerfData();')
      action_runner.Wait(SESSION_WAIT_TIME)


class CastFlingingPage(MediaRouterBasePage):
  """Cast page to fling a video to Chromecast device."""

  def __init__(self, page_set):
    super(CastFlingingPage, self).__init__(
        page_set=page_set,
        url='file://test_site/basic_test.html#flinging',
        shared_page_state_class=SharedState,
        name='basic_test.html#flinging')

  def RunPageInteractions(self, action_runner):
    sink_name = self._GetOSEnviron('RECEIVER_NAME')

    # Enable Cast and start to discover all sinks.
    action_runner.tab.EnableCast()
    # Wait for 5s after Chrome is opened in order to get consistent results.
    action_runner.Wait(5)
    with action_runner.CreateInteraction('flinging'):
      action_runner.tab.StopCasting(sink_name)

      self._WaitForResult(
          action_runner,
          lambda: action_runner.EvaluateJavaScript('initialized'),
          'Failed to initialize',
          timeout=30)

      # Wait for the sinks to appear.
      self.WaitForSink(
          action_runner, sink_name,
          'Targeted receiver "%s" did not showed up. Sink List: %s' % (
              sink_name, str(action_runner.tab.GetCastSinks())))
      action_runner.tab.SetCastSinkToUse(sink_name)

      # Start session
      action_runner.TapElement(selector='#start_session_button')
      action_runner.Wait(WAIT_TIME_SEC)
      self._WaitForResult(
        action_runner,
        lambda: action_runner.EvaluateJavaScript('currentSession'),
         'Failed to start session',
         timeout=WAIT_TIME_SEC)

      # Load Media
      self.ExecuteAsyncJavaScript(
          action_runner,
          js_template.Render(
              'loadMedia({{ url }});', url=self._GetOSEnviron('VIDEO_URL')),
          lambda: action_runner.EvaluateJavaScript('currentMedia'),
          'Failed to load media',
          timeout=120)

      action_runner.Wait(5)
      action_runner.ExecuteJavaScript('collectPerfData();')
      action_runner.Wait(SESSION_WAIT_TIME)
      # Stop session
      self.ExecuteAsyncJavaScript(
          action_runner,
          'stopSession();',
          lambda: not action_runner.EvaluateJavaScript('currentSession'),
          'Failed to stop session',
          timeout=60, retry=3)


class CastMirroringPage(MediaRouterBasePage):
  """Cast page to mirror a tab to Chromecast device."""

  def __init__(self, page_set):
    super(CastMirroringPage, self).__init__(
        page_set=page_set,
        url='file://test_site/mirroring.html',
        shared_page_state_class=SharedState,
        name='mirroring.html')

  def RunPageInteractions(self, action_runner):
    sink_name = self._GetOSEnviron('RECEIVER_NAME')

    # Enable Cast and start to discover all sinks.
    action_runner.tab.EnableCast()
    # Wait for 5s after Chrome is opened in order to get consistent results.
    action_runner.Wait(5)
    with action_runner.CreateInteraction('mirroring'):
      action_runner.tab.StopCasting(sink_name)

      # Wait for the sinks to appear.
      self.WaitForSink(
          action_runner, sink_name,
          'Targeted receiver "%s" did not showed up. Sink List: %s' % (
              sink_name, str(action_runner.tab.GetCastSinks())))

      # Start session
      action_runner.Wait(WAIT_TIME_SEC)
      action_runner.tab.StartTabMirroring(sink_name)

      # Make sure the route is created.
      if action_runner.tab.GetCastIssue():
        raise RuntimeError(action_runner.tab.GetCastIssue())
      action_runner.ExecuteJavaScript('collectPerfData();')
      action_runner.Wait(SESSION_WAIT_TIME)
      action_runner.tab.StopCasting(sink_name)


class MediaRouterCPUMemoryPageSet(story.StorySet):
  """Pageset for media router CPU/memory usage tests."""

  def __init__(self):
    super(MediaRouterCPUMemoryPageSet, self).__init__(
        cloud_storage_bucket=story.PARTNER_BUCKET)
    self.AddStory(CastIdlePage(self))
    self.AddStory(CastFlingingPage(self))
    self.AddStory(CastMirroringPage(self))


class CPUMemoryPageSet(story.StorySet):
  """Pageset to get baseline CPU and memory usage."""

  def __init__(self):
    super(CPUMemoryPageSet, self).__init__(
        cloud_storage_bucket=story.PARTNER_BUCKET)
    self.AddStory(CastIdlePage(self, 'basic_test.html#no_component_extension'))
