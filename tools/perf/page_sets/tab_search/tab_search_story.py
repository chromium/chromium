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
    'www.indeed.com',
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
    'cnbc.com',
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

TAB_SEARCH_URL = 'chrome://tab-search/'


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
    if self.WAIT_FOR_NETWORK_QUIESCENCE:
      for i, url in enumerate(url_list):
        try:
          tabs[i].action_runner.WaitForNetworkQuiescence()
        except py_utils.TimeoutException:
          logging.warning('WaitForNetworkQuiescence() timeout, url[%d]: %s' %
                          (i, url))

  def RunPageInteractions(self, action_runner):
    tabs = action_runner.tab.browser.tabs

    # Open Tab Search bubble.
    action_runner.tab.browser.supports_inspecting_webui = True
    action_runner.tab.browser.ExecuteBrowserCommand('openTabSearch')
    # Wait for Tab Search bubble to be inspectable.
    py_utils.WaitFor(
        lambda: any([True for tab in tabs if tab.url == TAB_SEARCH_URL]), 10)

    # Wait for Tab Search bubble to load.
    tab = next(iter([tab for tab in tabs if tab.url == TAB_SEARCH_URL]))
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

  def CloseAndOpenLoading(self, action_runner):
    action_runner.Wait(1)
    action_runner.tab.browser.ExecuteBrowserCommand('closeTabSearch')
    action_runner.Wait(1)
    tabs = action_runner.tab.browser.tabs
    i = 0
    for url in self.URL_LIST2:
      tabs[i].Navigate('https://' + url)
      i = i + 1
    action_runner.tab.browser.ExecuteBrowserCommand('openTabSearch')
    action_runner.Wait(5)

  def ScrollUpAndDown(self, action_runner):
    action_runner.Wait(1)
    self.StartMeasuringFrameTime(action_runner,
                                 'frame_time_on_first_scroll_down')
    action_runner.ScrollElement(element_function=SCROLL_ELEMENT_FUNCTION)
    self.StartMeasuringFrameTime(action_runner, 'frame_time_on_first_scroll_up')
    action_runner.ScrollElement(element_function=SCROLL_ELEMENT_FUNCTION,
                                direction='up')
    self.StartMeasuringFrameTime(action_runner,
                                 'frame_time_on_second_scroll_down')
    action_runner.ScrollElement(element_function=SCROLL_ELEMENT_FUNCTION)
    self.StopMeasuringFrameTime(action_runner)
    action_runner.Wait(1)

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


class TabSearchStoryTop10Loading(TabSearchStory):
  NAME = 'tab_search:top10:loading:2020'
  URL_LIST = TOP_URL[:10]
  URL = 'https://' + URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = False


class TabSearchStoryTop50Loading(TabSearchStory):
  NAME = 'tab_search:top50:loading:2020'
  URL_LIST = TOP_URL[:50]
  URL = 'https://' + URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = False


class TabSearchStoryTop100Loading(TabSearchStory):
  NAME = 'tab_search:top100:loading:2020'
  URL_LIST = TOP_URL[:50] * 2
  URL = 'https://' + URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = False


class TabSearchStoryCloseAndOpen(TabSearchStory):
  NAME = 'tab_search:close_and_open:2020'
  URL_LIST = TOP_URL[:10]
  URL = 'https://' + URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = True

  def InteractWithPage(self, action_runner):
    self.CloseAndOpen(action_runner)


class TabSearchStoryCloseAndOpenLoading(TabSearchStory):
  NAME = 'tab_search:close_and_open:loading:2020'
  URL_LIST = TOP_URL[:10]
  URL_LIST2 = TOP_URL[10:20]
  URL = 'https://' + URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = False

  def InteractWithPage(self, action_runner):
    self.CloseAndOpenLoading(action_runner)


class TabSearchStoryScrollUpAndDown(TabSearchStory):
  NAME = 'tab_search:scroll_up_and_down:2020'
  URL_LIST = TOP_URL[:50] * 2
  URL = 'https://' + URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = False

  def InteractWithPage(self, action_runner):
    self.ScrollUpAndDown(action_runner)


SCROLL_ELEMENT_FUNCTION = '''
document.querySelector('tab-search-app').shadowRoot.getElementById('tabsList')
'''

MEASURE_FRAME_TIME_SCRIPT = '''
window.__webui_startMeasuringFrameTime = function(name) {
  if (window.__webui_onRequestAnimationFrame) {
    window.__webui_stopMeasuringFrameTime();
  }
  window.__webui_onRequestAnimationFrame = function() {
    const now = performance.now();
    if (window.__webui_lastAnimationFrameTime) {
      performance.mark(
          `${name}:${now - window.__webui_lastAnimationFrameTime}:benchmark_value`);
    }
    window.__webui_lastAnimationFrameTime = now;
    if (window.__webui_onRequestAnimationFrame) {
      window.__webui_lastRequestId = requestAnimationFrame(
          window.__webui_onRequestAnimationFrame);
    }
  }
  window.__webui_lastRequestId = requestAnimationFrame(
      window.__webui_onRequestAnimationFrame);
}

window.__webui_stopMeasuringFrameTime = function() {
  if (window.__webui_lastRequestId) {
    cancelAnimationFrame(window.__webui_lastRequestId);
  }
  window.__webui_lastRequestId = null;
  window.__webui_onRequestAnimationFrame = null;
  window.__webui_lastAnimationFrameTime = null;
}
'''

START_MEASURING_FRAME_TIME = '''
window.__webui_startMeasuringFrameTime('%s')
'''

STOP_MEASURING_FRAME_TIME = '''
window.__webui_stopMeasuringFrameTime()
'''
