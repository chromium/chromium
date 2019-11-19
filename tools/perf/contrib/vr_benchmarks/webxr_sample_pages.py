# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from contrib.vr_benchmarks.vr_sample_page import XrSamplePage
from contrib.vr_benchmarks.vr_story_set import VrStorySet


class WebXrSamplePage(XrSamplePage):

  def __init__(self, page_set, url_parameters, sample_page,
      extra_browser_args=None):
    super(WebXrSamplePage, self).__init__(
        sample_page=sample_page,
        page_set=page_set,
        url_parameters=url_parameters,
        extra_browser_args=extra_browser_args)

  def RunPageInteractions(self, action_runner):
    # The user-visible text differs per-page (e.g. "Enter VR"), but the
    # element's "title" attribute is currently always "Enter XR".
    action_runner.WaitForElement(selector='button[title="Enter XR"]')
    action_runner.TapElement(selector='button[title="Enter XR"]')
    action_runner.MeasureMemory(True)
    # We don't want to be in VR or on a page with a WebGL canvas at the end of
    # the test, as this generates unnecessary heat while the trace data is being
    # processed, so navigate to a blank page if we're on a platform that cares
    # about the heat generation.
    if self._shared_page_state.ShouldNavigateToBlankPageBeforeFinishing():
      action_runner.Navigate("about:blank")


class WebXrSamplePageSet(VrStorySet):
  """A page set using the official WebXR samples with settings tweaked."""

  def __init__(self, use_fake_pose_tracker=True):
    super(WebXrSamplePageSet, self).__init__(
        use_fake_pose_tracker=use_fake_pose_tracker)

    # Test cases that use the synthetic cube field page
    cube_test_cases = [
      # Standard sample app with no changes.
      ['framebufferScale=1.0'],
      # Increased render scale.
      ['framebufferScale=1.4'],
      # Default render scale, increased load.
      ['framebufferScale=1.0', 'heavyGpu=1', 'cubeScale=0.2', 'workTime=5'],
      # Further increased load.
      ['framebufferScale=1.0', 'heavyGpu=1', 'cubeScale=0.3', 'workTime=10'],
      # Absurd load for fill-rate testing. Only half the cube sea is rendered,
      # and the page automatically rotates the view between the rendered and
      # unrendered halves.
      ['frameBufferScale=1.4', 'heavyGpu=1', 'cubeScale=0.4', 'workTime=4',
       'halfOnly=1', 'autorotate=1'],
    ]

    for url_parameters in cube_test_cases:
      self.AddStory(WebXrSamplePage(self, url_parameters, 'tests/cube-sea'))
