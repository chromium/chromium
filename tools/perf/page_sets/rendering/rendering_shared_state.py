# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging

from page_sets.rendering import story_tags
from telemetry.page import shared_page_state


class NoSwiftShaderAssertionFailure(AssertionError):
  pass


class RenderingSharedState(shared_page_state.SharedPageState):
  def CanRunOnBrowser(self, browser_info, page):
    if page.TAGS and story_tags.REQUIRED_WEBGL in page.TAGS:
      assert hasattr(page, 'skipped_gpus')

      if not browser_info.HasWebGLSupport():
        logging.warning('Browser does not support webgl, skipping test')
        return False

      # Check the skipped GPUs list.
      # Requires the page provide a "skipped_gpus" property.
      browser = browser_info.browser
      system_info = browser.GetSystemInfo()
      if system_info:
        gpu_info = system_info.gpu
        gpu_vendor = self._GetGpuVendorString(gpu_info)
        if gpu_vendor in page.skipped_gpus:
          return False

    return True

  def _GetGpuVendorString(self, gpu_info):
    if gpu_info:
      primary_gpu = gpu_info.devices[0]
      if primary_gpu:
        vendor_string = primary_gpu.vendor_string.lower()
        vendor_id = primary_gpu.vendor_id
        if vendor_string:
          return vendor_string.split(' ')[0]
        if vendor_id == 0x10DE:
          return 'nvidia'
        if vendor_id == 0x1002:
          return 'amd'
        if vendor_id == 0x8086:
          return 'intel'
        if vendor_id == 0x15AD:
          return 'vmware'

    return 'unknown_gpu'

  def WillRunStory(self, story):
    super(RenderingSharedState, self).WillRunStory(story)
    if not self._finder_options.allow_software_compositing:
      self._EnsureNotSwiftShader()
    if story.TAGS and story_tags.KEY_IDLE_POWER in story.TAGS:
      self._EnsureScreenOn()

  def DidRunStory(self, results):
    if (self.current_page.TAGS
        and story_tags.MOTIONMARK in self.current_page.TAGS):
      unit = 'unitless_biggerIsBetter'
      results.AddMeasurement('motionmark', unit, [self.current_page.score])
      results.AddMeasurement('motionmarkLower', unit,
                             [self.current_page.scoreLowerBound])
      results.AddMeasurement('motionmarkUpper', unit,
                             [self.current_page.scoreUpperBound])

      stories = self.current_page.stories
      storyScores = self.current_page.storyScores
      lowerBounds = self.current_page.storyScoreLowerBounds
      upperBounds = self.current_page.storyScoreUpperBounds
      score_index = 0
      for story in stories:
        results.AddMeasurement(story, unit, storyScores[score_index])
        results.AddMeasurement(story + ' Lower', unit, lowerBounds[score_index])
        results.AddMeasurement(story + ' Upper', unit, upperBounds[score_index])
        score_index += 1

    if (self.current_page.TAGS and
        story_tags.KEY_IDLE_POWER in self.current_page.TAGS):
      try:
        super(RenderingSharedState, self).DidRunStory(results)
      finally:
        self._EnsureScreenOn()
    else:
      super(RenderingSharedState, self).DidRunStory(results)

  def _EnsureScreenOn(self):
    self.platform.android_action_runner.TurnScreenOn()

  def _EnsureNotSwiftShader(self):
    system_info = self.browser.GetSystemInfo()
    if system_info:
      for device in system_info.gpu.devices:
        if device.device_string == u'Google SwiftShader':
          raise NoSwiftShaderAssertionFailure(
                'SwiftShader should not be used for rendering benchmark, since '
                'the metrics produced from that do not reflect the real '
                'performance for a lot of metrics.')


class DesktopRenderingSharedState(RenderingSharedState):
  _device_type = 'desktop'


class MobileRenderingSharedState(RenderingSharedState):
  _device_type = 'mobile'
