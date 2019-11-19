# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from contrib.vr_benchmarks.vr_sample_page import VrSamplePage
from contrib.vr_benchmarks.vr_story_set import VrStorySet


class WebVrSamplePage(VrSamplePage):

  def __init__(self, page_set, url_parameters, sample_page,
      extra_browser_args=None,):
    super(WebVrSamplePage, self).__init__(
        sample_page=sample_page,
        page_set=page_set,
        url_parameters=url_parameters,
        extra_browser_args=extra_browser_args)

  def RunPageInteractions(self, action_runner):
    action_runner.TapElement(selector='canvas[id="webgl-canvas"]')
    action_runner.MeasureMemory(True)
    # We don't want to be in VR or on a page with a WebGL canvas at the end of
    # the test, as this generates unnecessary heat while the trace data is being
    # processed, so navigate to a blank page if we're on a platform that cares
    # about the heat generation.
    if self._shared_page_state.ShouldNavigateToBlankPageBeforeFinishing():
      action_runner.Navigate("about:blank")


class WebVrSamplePageSet(VrStorySet):
  """A page set using the official WebVR sample with settings tweaked."""

  def __init__(self, use_fake_pose_tracker=True):
    super(WebVrSamplePageSet, self).__init__(
        use_fake_pose_tracker=use_fake_pose_tracker)

    # Test cases that use the synthetic cube field page
    cube_test_cases = [
      # Standard sample app with no changes
      ['canvasClickPresents=1', 'renderScale=1'],
      # Increased render scale
      ['canvasClickPresents=1', 'renderScale=1.5'],
      # Default render scale, increased load
      ['canvasClickPresents=1', 'renderScale=1', 'heavyGpu=1', 'cubeScale=0.2',
          'workTime=5'],
      # Further increased load
      ['canvasClickPresents=1', 'renderScale=1', 'heavyGpu=1', 'cubeScale=0.3',
          'workTime=10'],
      # Absurd load for fill-rate testing
      ['canvasClickPresents=1', 'renderScale=1.6', 'heavyGpu=1',
          'cubeScale=0.3', 'workTime=4'],
    ]

    for url_parameters in cube_test_cases:
      # Standard set of pages with defaults
      self.AddStory(WebVrSamplePage(self, url_parameters, 'test-slow-render'))
      # Set of pages with standardized render size
      self.AddStory(WebVrSamplePage(self, url_parameters + ['standardSize=1'],
          'test-slow-render'))

    # Test cases that use the 360 video page
    video_test_cases = [
      # Test using the default, low resolution video
      ['canvasClickPresents=1'],
    ]

    for url_parameters in video_test_cases:
      self.AddStory(WebVrSamplePage(self, url_parameters, 'XX-360-video'))
