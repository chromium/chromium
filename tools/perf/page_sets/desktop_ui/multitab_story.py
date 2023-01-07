# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import py_utils
from page_sets.desktop_ui import desktop_ui_shared_state
from page_sets.desktop_ui.js_utils import MEASURE_FRAME_TIME_SCRIPT, \
    START_MEASURING_FRAME_TIME, STOP_MEASURING_FRAME_TIME
from telemetry.page import page


class MultiTabStory(page.Page):
  """Base class for stories to open tabs with a list of urls"""

  def __init__(self, story_set, extra_browser_args=None):
    tags = []
    if hasattr(self, 'TAGS'):
      for t in self.TAGS:
        tags.append(t.name)
    super(MultiTabStory, self).__init__(
        url=self.URL,
        name=self.NAME,
        tags=tags,
        page_set=story_set,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=desktop_ui_shared_state.DesktopUISharedState)
    self._devtools = None

  def RunNavigateSteps(self, action_runner):
    url_list = self.URL_LIST
    tabs = action_runner.tab.browser.tabs
    if len(url_list) > 0:
      tabs[0].Navigate(url_list[0])
    for url in url_list[1:]:
      new_tab = tabs.New()
      new_tab.Navigate(url)
    if self.WAIT_FOR_NETWORK_QUIESCENCE:
      for i, url in enumerate(url_list):
        try:
          tabs[i].action_runner.WaitForNetworkQuiescence()
        except py_utils.TimeoutException:
          logging.warning('WaitForNetworkQuiescence() timeout, url[%d]: %s' %
                          (i, url))
    self._devtools = action_runner.tab.browser.GetUIDevtools()

  def StartMeasuringFrameTime(self, action_runner, name):
    action_runner.ExecuteJavaScript(MEASURE_FRAME_TIME_SCRIPT)
    action_runner.ExecuteJavaScript(START_MEASURING_FRAME_TIME % name)

  def StopMeasuringFrameTime(self, action_runner):
    action_runner.ExecuteJavaScript(STOP_MEASURING_FRAME_TIME)

  def WillStartTracing(self, chrome_trace_config):
    chrome_trace_config.category_filter.AddIncludedCategory('browser')
    chrome_trace_config.category_filter.AddIncludedCategory('blink.user_timing')

  def GetExtraTracingMetrics(self):
    return ['customMetric']
