# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import story
from telemetry import page
from contrib.vr_benchmarks import (shared_vr_page_state as vr_state)
from contrib.vr_benchmarks.vr_story_set import VrStorySet

class WebVrWprPage(page.Page):
  """Class for running a story on a WebVR WPR page."""
  def __init__(self, page_set, url, name, interaction_function,
      extra_browser_args=None):
    """
    Args:
      page_set: The StorySet the WebVrWprPage is being added to
      url: The URL to navigate to for the story
      name: The name of the story
      interaction_function: A pointer to a function that takes an ActionRunner
          and boolean indicating whether a WPR archive is being recorded. Meant
          to handle webpage-specific VR entry
      extra_browser_args: Extra browser args that are simply forwarded to
          page.Page
    """
    super(WebVrWprPage, self).__init__(
        url=url,
        page_set=page_set,
        name=name,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=vr_state.SharedVrPageStateFactory)
    self._shared_page_state = None
    self._interaction_function = interaction_function

  def RunPageInteractions(self, action_runner):
    if self._interaction_function:
      self._interaction_function(action_runner, self.recording_wpr)

    action_runner.MeasureMemory(True)
    if self._shared_page_state.ShouldNavigateToBlankPageBeforeFinishing():
      action_runner.Navigate("about:blank")

  def Run(self, shared_state):
    self._shared_page_state = shared_state
    super(WebVrWprPage, self).Run(shared_state)

  @property
  def platform(self):
    return self._shared_page_state.platform

  @property
  def recording_wpr(self):
    return self._shared_page_state.recording_wpr


class WebVrWprPageSet(VrStorySet):
  """A page set using live WebVR sites recorded using WPR."""

  def __init__(self, use_fake_pose_tracker=True):
    super(WebVrWprPageSet, self).__init__(
        archive_data_file='data/webvr_wpr.json',
        cloud_storage_bucket=story.PARTNER_BUCKET,
        use_fake_pose_tracker=use_fake_pose_tracker)

    # View the Pirates: Dock model on Sketchfab
    def SketchfabInteraction(action_runner, _):
      action_runner.WaitForNetworkQuiescence(timeout_in_seconds=20)
      action_runner.WaitForElement(selector='a[data-tooltip="View in VR"]')
      action_runner.TapElement(selector='a[data-tooltip="View in VR"]')
    self.AddStory(WebVrWprPage(
        self,
        'https://sketchfab.com/models/uFqGJrS9ZjVr9Myk9kg4fubPNPz',
        'sketchfab_pirates_dock',
        SketchfabInteraction))

    # Watch part of the Invasion Episode 1 video on With.in
    def WithinInvasionInteraction(action_runner, recording_wpr):
      action_runner.WaitForNetworkQuiescence()
      action_runner.TapElement(selector='div.play-button')
      action_runner.TapElement(selector='div.right.stereo')
      # Make sure we get enough streaming video during WPR recording to not run
      # into issues when actually running the benchmark
      if recording_wpr:
        action_runner.Wait(30)
    self.AddStory(WebVrWprPage(
        self,
        # Access the video directly to more easily set resolution and avoid
        # iframe weirdness
        'https://player.with.in/embed/?id=272&resolution=2880&forced=false&'
        'autoplay=true&t=0&internal=true',
        'within_invasion_ep_1',
        WithinInvasionInteraction))

    # Watch a girl running through a giant forest (I think) in Under Neon Lights
    # Note that this is semi-broken in that it doesn't move away from the
    # opening when you enter VR, but we're still viewing a relatively complex
    # WebGL scene, so it's still useful for perf testing
    def UnderNeonLightsInteraction(action_runner, _):
      action_runner.WaitForNetworkQuiescence(timeout_in_seconds=30)
      # The VR button doesn't have any unique ID or anything, so instead select
      # based on the unique text in a child div
      action_runner.WaitForElement(text='Start in VR')
      action_runner.TapElement(text='Start in VR')
    self.AddStory(WebVrWprPage(
        self,
        # Access the content directly to avoid iframe weirdness
        'https://player.with.in/embed/?id=541&resolution=1920&forced=false&'
        'autoplay=true&t=0&internal=true',
        'under_neon_lights',
        UnderNeonLightsInteraction))


class WebVrLivePageSet(WebVrWprPageSet):
  """A superset of the  WPR page set.

  Also contains sites that we would like to run with WPR, but that interact
  badly when replayed. So, access the live version instead.
  """
  def __init__(self, use_fake_pose_tracker=True):
    super(WebVrLivePageSet, self).__init__(
        use_fake_pose_tracker=use_fake_pose_tracker)

    # Look at "randomly" generated (constant seed) geometry in Mass Migrations
    # Not usable via WPR due to it often not submitting frames while using WPR
    def MassMigrationsInteraction(action_runner, _):
      action_runner.WaitForNetworkQuiescence()
      # All DOM elements seem to be present on the page from the start, so
      # instead wait until the button is actually visible
      action_runner.WaitForJavaScriptCondition(
          condition='document.querySelector(\'div[id="footer"]\').style.display'
                    '== "block"')
      action_runner.TapElement(selector='a[id="vr"]')
    self.AddStory(WebVrWprPage(
        self,
        # The /iaped is necessary to keep the geometry constant, as it acts as
        # the seed for the generator - just visiting the site randomly generates
        # geometry
        'https://massmigrations.com/iaped',
        'mass_migrations',
        MassMigrationsInteraction))

    # Watch dancing polyhedrons in Dance Tonite
    # Not usable via WPR due to weird rendering issues (incorrect colors,
    # missing geometry, etc.) that are present when using WPR
    def DanceToniteInteraction(action_runner, _):
      action_runner.WaitForNetworkQuiescence()
      action_runner.WaitForElement(selector='div.button-play')
      action_runner.TapElement(selector='div.button-play')
      action_runner.WaitForNetworkQuiescence()
      # The VR entry button has no unique text, ID, etc. but does have a child
      # SVG with a child path with a unique "d" attribute. It's super long, so
      # only match against one part of it
      action_runner.WaitForElement(selector='path[d~="M52.6"]')
      action_runner.TapElement(selector='path[d~="M52.6"]')
    self.AddStory(WebVrWprPage(
        self,
        'https://tonite.dance',
        'dance_tonite',
        DanceToniteInteraction))
