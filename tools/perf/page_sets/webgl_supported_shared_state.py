# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging

from telemetry.page import shared_page_state

class WebGLSupportedSharedState(shared_page_state.SharedPageState):
  def CanRunOnBrowser(self, browser_info, page):
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
