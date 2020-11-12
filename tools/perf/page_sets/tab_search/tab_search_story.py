# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import py_utils
from telemetry.page import page
from telemetry.internal.actions.action_runner import ActionRunner

TOP_URL = [
    'google.com',
    'youtube.com',
    'amazon.com',
    'facebook.com',
    'zoom.us',
    'yahoo.com',
    'reddit.com',
    'wikipedia.org',
    'myshopify.com',
    'ebay.com',
    'instructure.com',
    'office.com',
    'netflix.com',
    'bing.com',
    'live.com',
    'microsoft.com',
    'espn.com',
    'twitch.tv',
    'blogger.com',
    'instagram.com',
    'mozilla.org',
    'cnn.com',
    'apple.com',
    'zillow.com',
    'etsy.com',
    'chase.com',
    'nytimes.com',
    'linkedin.com',
    'dropbox.com',
    'adobe.com',
    'okta.com',
    'craigslist.org',
    'twitter.com',
    'walmart.com',
    'aliexpress.com',
    'github.com',
    'vimeo.com',
    'quizlet.com',
    'tmall.com',
    'imgur.com',
    'wellsfargo.com',
    'hulu.com',
    'imdb.com',
    'salesforce.com',
    'homedepot.com',
    'indeed.com',
    'foxnews.com',
    'msn.com',
    'spotify.com',
    'whatsapp.com',
]


class TabSearchStory(page.Page):
  """Base class for tab search stories"""

  def __init__(self, story_set, extra_browser_args=None):
    super(TabSearchStory, self).__init__(url=self.URL,
                                         name=self.NAME,
                                         page_set=story_set,
                                         extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    url_list = self.URL_LIST
    tabs = action_runner.tab.browser.tabs
    tabs[0].Navigate('https://' + url_list[0])
    for url in url_list[1:]:
      new_tab = tabs.New()
      new_tab.Navigate('https://' + url)
      action_runner.Wait(0.2)
    if self.WAIT_FOR_NETWORK_QUIESCENCE:
      for i, url in enumerate(url_list):
        try:
          tabs[i].action_runner.WaitForNetworkQuiescence()
        except py_utils.TimeoutException:
          logging.warning('WaitForNetworkQuiescence() timeout, url[%d]: %s' %
                          (i, url))

  def RunPageInteractions(self, action_runner):
    tabs = action_runner.tab.browser.tabs
    tabs_len = len(tabs)

    # Open Tab Search bubble.
    action_runner.tab.browser.supports_inspecting_webui = True
    action_runner.tab.browser.ExecuteBrowserCommand('openTabSearch')
    # Wait for Tab Search bubble to be inspectable.
    py_utils.WaitFor(lambda: len(tabs) > tabs_len, 10)

    # Wait for Tab Search bubble to load.
    tab = tabs[-1]
    action_runner = ActionRunner(
        tab)  # Recreate action_runner for Tab Search bubble.
    tab.WaitForDocumentReadyStateToBeComplete()

    # Send key navigation to Tab Search bubble.
    self.InteractWithPage(action_runner)

  def InteractWithPage(self, action_runner):
    self.ScrollTabs(action_runner)
    self.SearchTabs(action_runner)
    self.CloseTab(action_runner)

  def ScrollUp(self, action_runner):
    action_runner.Wait(1)
    # Scroll to the bottom of the list.
    action_runner.PressKey('ArrowUp')
    action_runner.Wait(1)

  def SearchTabs(self, action_runner):
    action_runner.Wait(1)
    action_runner.EnterText('o')
    action_runner.Wait(2)
    action_runner.PressKey('Backspace')
    action_runner.Wait(1)

  def CloseTab(self, action_runner):
    action_runner.Wait(1)
    # Tab to the close button of the 2nd tab.
    action_runner.PressKey('Tab', repeat_count=4, repeat_delay_ms=500)
    action_runner.PressKey(' ')
    action_runner.Wait(1)

  def ScrollTabs(self, action_runner):
    action_runner.Wait(1)
    self.StartMeasuringFrameTime(action_runner, 'frame_time_on_scroll')
    action_runner.ScrollElement(element_function=SCROLL_ELEMENT_FUNCTION)
    self.StopMeasuringFrameTime(action_runner)
    action_runner.Wait(1)

  def CloseAndOpen(self, action_runner):
    action_runner.Wait(1)
    action_runner.tab.browser.ExecuteBrowserCommand('closeTabSearch')
    action_runner.Wait(1)
    action_runner.tab.browser.ExecuteBrowserCommand('openTabSearch')
    action_runner.Wait(5)

  def StartMeasuringFrameTime(self, action_runner, name):
    action_runner.ExecuteJavaScript(MEASURE_FRAME_TIME_SCRIPT)
    action_runner.ExecuteJavaScript(START_MEASURING_FRAME_TIME % name)

  def StopMeasuringFrameTime(self, action_runner):
    action_runner.ExecuteJavaScript(STOP_MEASURING_FRAME_TIME)


class TabSearchStoryTop10(TabSearchStory):
  NAME = 'tab_search:top10:2020'
  URL_LIST = TOP_URL[:10]
  URL = 'https://' + URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = True


class TabSearchStoryTop50(TabSearchStory):
  NAME = 'tab_search:top50:2020'
  URL_LIST = TOP_URL[:50]
  URL = 'https://' + URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = True


class TabSearchStoryTop100(TabSearchStory):
  NAME = 'tab_search:top100:2020'
  URL_LIST = TOP_URL[:50] * 2
  URL = 'https://' + URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = True


SCROLL_ELEMENT_FUNCTION = '''
document.querySelector('tab-search-app').shadowRoot.getElementById('tabsList')
.shadowRoot.getElementById('container')
'''

MEASURE_FRAME_TIME_SCRIPT = '''
window.__webui_startMeasuringFrameTime = function(name) {
  if (!window.__webui_onRequestAnimationFrame) {
    window.__webui_onRequestAnimationFrame = function() {
      performance.mark(name + ':benchmark_end');
      if (window.__webui_onRequestAnimationFrame) {
        requestAnimationFrame(window.__webui_onRequestAnimationFrame);
        performance.mark(name + ':benchmark_begin')
      }
    }
    performance.mark(name + ':benchmark_begin')
    requestAnimationFrame(window.__webui_onRequestAnimationFrame);
  }
}

window.__webui_stopMeasuringFrameTime = function() {
  window.__webui_onRequestAnimationFrame = null;
}
'''

START_MEASURING_FRAME_TIME = '''
window.__webui_startMeasuringFrameTime('%s')
'''

STOP_MEASURING_FRAME_TIME = '''
window.__webui_stopMeasuringFrameTime()
'''
