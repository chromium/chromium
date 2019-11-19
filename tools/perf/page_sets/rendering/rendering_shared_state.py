# Copyright 2018 The Chromium Authors. All rights reserved.
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
        elif vendor_id == 0x10DE:
          return 'nvidia'
        elif vendor_id == 0x1002:
          return 'amd'
        elif vendor_id == 0x8086:
          return 'intel'
        elif vendor_id == 0x15AD:
          return 'vmware'

    return 'unknown_gpu'

  def WillRunStory(self, page):
    super(RenderingSharedState, self).WillRunStory(page)
    self._EnsureNotSwiftShader()
    if page.TAGS and story_tags.KEY_IDLE_POWER in page.TAGS:
      self._EnsureScreenOn()

  def DidRunStory(self, results):
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
