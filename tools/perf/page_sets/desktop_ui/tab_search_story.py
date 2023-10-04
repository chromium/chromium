# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import py_utils

from page_sets.desktop_ui.browser_element_identifiers import \
    kTabSearchButtonElementId
from page_sets.desktop_ui.custom_metric_utils import SetMetricNames
from page_sets.desktop_ui.js_utils import MEASURE_JS_MEMORY
from page_sets.desktop_ui.multitab_story import MultiTabStory
from page_sets.desktop_ui.ui_devtools_utils import ClickOn
from page_sets.desktop_ui.url_list import TOP_URL
from page_sets.desktop_ui.webui_utils import Inspect
from page_sets.desktop_ui import story_tags

TAB_SEARCH_BENCHMARK_UMA = [
    'Tabs.TabSearch.CloseAction',
    'Tabs.TabSearch.Mojo.SwitchToTab',
    'Tabs.TabSearch.Mojo.SwitchToTab.IsOverlap',
    'Tabs.TabSearch.Mojo.TabUpdated',
    'Tabs.TabSearch.Mojo.TabUpdated.IsOverlap',
    'Tabs.TabSearch.NumTabsClosedPerInstance',
    'Tabs.TabSearch.NumTabsOnOpen',
    'Tabs.TabSearch.NumWindowsOnOpen',
    'Tabs.TabSearch.OpenAction',
    'Tabs.TabSearch.PageHandlerConstructionDelay',
    'Tabs.TabSearch.WebUI.InitialTabsRenderTime',
    'Tabs.TabSearch.WebUI.LoadCompletedTime',
    'Tabs.TabSearch.WebUI.LoadDocumentTime',
    'Tabs.TabSearch.WebUI.SearchAlgorithmDuration',
    'Tabs.TabSearch.WebUI.TabListDataReceived',
    'Tabs.TabSearch.WebUI.TabListDataReceived2',
    'Tabs.TabSearch.WebUI.TabListDataReceived2.IsOverlap',
    'Tabs.TabSearch.WebUI.TabSwitchAction',
    'Tabs.TabSearch.WindowDisplayedDuration2',
    'Tabs.TabSearch.WindowTimeToShowCachedWebView',
    'Tabs.TabSearch.WindowTimeToShowUncachedWebView',
]

TAB_SEARCH_CUSTOM_METRIC_NAMES = [
    'TabSearchPageHandler:GetProfileTabs',
    'TabSearchPageHandler:TabChangedAt',
]

TAB_SEARCH_URL = 'chrome://tab-search.top-chrome/'


class TabSearchStory(MultiTabStory):
  """Base class for tab search stories"""

  def RunPageInteractions(self, action_runner):
    SetMetricNames(action_runner, TAB_SEARCH_CUSTOM_METRIC_NAMES)
    self.ToggleTabSearch()
    action_runner = Inspect(action_runner.tab.browser, TAB_SEARCH_URL)
    action_runner.ExecuteJavaScript(MEASURE_JS_MEMORY %
                                    'tab_search:used_js_heap_size_begin')
    self.InteractWithPage(action_runner)
    action_runner.ExecuteJavaScript(MEASURE_JS_MEMORY %
                                    'tab_search:used_js_heap_size_end')

  def ToggleTabSearch(self, index=0):
    # Click a little bit into the button instead of the extreme top-left corner.
    # The button's corners are not included in its hit test mask.
    ClickOn(self._devtools,
            element_id=kTabSearchButtonElementId,
            index=index,
            x=10,
            y=10)

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
    self.StartMeasuringFrameTime(action_runner,
                                 'tab_search:frame_time_on_scroll')
    action_runner.ScrollElement(element_function=SCROLL_ELEMENT_FUNCTION)
    self.StopMeasuringFrameTime(action_runner)
    action_runner.Wait(1)

  def CloseAndOpen(self, action_runner):
    action_runner.Wait(1)
    self.ToggleTabSearch()
    action_runner.Wait(1)
    self.ToggleTabSearch()
    action_runner.Wait(5)

  def CloseAndOpenLoading(self, action_runner):
    action_runner.Wait(1)
    self.ToggleTabSearch()
    action_runner.Wait(1)
    tabs = action_runner.tab.browser.tabs
    i = 0
    for url in self.URL_LIST2:
      tabs[i].Navigate(url)
      i = i + 1
    self.ToggleTabSearch()
    action_runner.Wait(5)

  def ScrollUpAndDown(self, action_runner):
    action_runner.Wait(1)
    self.StartMeasuringFrameTime(action_runner,
                                 'tab_search:frame_time_on_first_scroll_down')
    action_runner.ScrollElement(element_function=SCROLL_ELEMENT_FUNCTION)
    self.StartMeasuringFrameTime(action_runner,
                                 'tab_search:frame_time_on_first_scroll_up')
    action_runner.ScrollElement(element_function=SCROLL_ELEMENT_FUNCTION,
                                direction='up')
    self.StartMeasuringFrameTime(action_runner,
                                 'tab_search:frame_time_on_second_scroll_down')
    action_runner.ScrollElement(element_function=SCROLL_ELEMENT_FUNCTION)
    self.StopMeasuringFrameTime(action_runner)
    action_runner.Wait(1)

  def WillStartTracing(self, chrome_trace_config):
    super(TabSearchStory, self).WillStartTracing(chrome_trace_config)
    chrome_trace_config.EnableUMAHistograms(*TAB_SEARCH_BENCHMARK_UMA)


class TabSearchRecentlyClosedStory(TabSearchStory):
  """Base class for tab search recently closed stories"""

  def RunNavigateSteps(self, action_runner):
    url_list = self.URL_LIST
    tabs = action_runner.tab.browser.tabs
    if len(url_list) > 0:
      tabs[0].Navigate(url_list[0])
    for url in url_list[1:]:
      new_tab = tabs.New()
      new_tab.Navigate(url)
      if self.WAIT_FOR_NETWORK_QUIESCENCE:
        try:
          new_tab.action_runner.WaitForNetworkQuiescence()
        except py_utils.TimeoutException:
          logging.warning('WaitForNetworkQuiescence() timeout: %s' % (url))
      new_tab.Close()
    self._devtools = action_runner.tab.browser.GetUIDevtools()

  def InteractWithPage(self, action_runner):
    self.SearchTabs(action_runner)
    self.ScrollTabs(action_runner)

  def SearchTabs(self, action_runner):
    action_runner.Wait(1)
    action_runner.EnterText('o')
    action_runner.Wait(2)


class TabSearchStoryRecentlyClosed10(TabSearchRecentlyClosedStory):
  NAME = 'tab_search:10_recently_closed'
  URL_LIST = ['chrome://version?q={}'.format(i) for i in range(10)]
  URL = URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = True


class TabSearchStoryRecentlyClosed50(TabSearchRecentlyClosedStory):
  NAME = 'tab_search:50_recently_closed'
  URL_LIST = ['chrome://version?q={}'.format(i) for i in range(50)]
  URL = URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = True


class TabSearchStoryRecentlyClosed100(TabSearchRecentlyClosedStory):
  NAME = 'tab_search:100_recently_closed'
  URL_LIST = ['chrome://version?q={}'.format(i) for i in range(100)]
  URL = URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = True


class TabSearchStoryTop10(TabSearchStory):
  NAME = 'tab_search:top10:2020'
  URL_LIST = TOP_URL[:10]
  URL = URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = True


class TabSearchStoryTop50(TabSearchStory):
  NAME = 'tab_search:top50:2020'
  URL_LIST = TOP_URL[:50]
  URL = URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = True


class TabSearchStoryTop10Loading(TabSearchStory):
  NAME = 'tab_search:top10:loading:2020'
  URL_LIST = TOP_URL[:10]
  URL = URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = False


class TabSearchStoryTop50Loading(TabSearchStory):
  NAME = 'tab_search:top50:loading:2020'
  URL_LIST = TOP_URL[:50]
  URL = URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = False


class TabSearchStoryCloseAndOpen(TabSearchStory):
  NAME = 'tab_search:close_and_open:2020'
  URL_LIST = TOP_URL[:10]
  URL = URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = True

  def InteractWithPage(self, action_runner):
    self.CloseAndOpen(action_runner)


class TabSearchStoryCloseAndOpenLoading(TabSearchStory):
  NAME = 'tab_search:close_and_open:loading:2020'
  URL_LIST = TOP_URL[:10]
  URL_LIST2 = TOP_URL[10:20]
  URL = URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = False

  def InteractWithPage(self, action_runner):
    self.CloseAndOpenLoading(action_runner)


class TabSearchStoryScrollUpAndDown(TabSearchStory):
  NAME = 'tab_search:scroll_up_and_down:2020'
  URL_LIST = TOP_URL[:50]
  URL = URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = False

  def InteractWithPage(self, action_runner):
    self.ScrollUpAndDown(action_runner)


class TabSearchStoryCleanSlate(TabSearchStory):
  NAME = 'tab_search:clean_slate'
  URL_LIST = []
  URL = 'about:blank'
  WAIT_FOR_NETWORK_QUIESCENCE = False
  TAGS = [story_tags.SMOKE_TEST]

  def InteractWithPage(self, action_runner):
    action_runner.Wait(1)


class TabSearchStoryMeasureMemory(TabSearchStory):
  URL_LIST = []
  URL = 'about:blank'
  WAIT_FOR_NETWORK_QUIESCENCE = False

  def WillStartTracing(self, chrome_trace_config):
    super(TabSearchStoryMeasureMemory,
          self).WillStartTracing(chrome_trace_config)
    chrome_trace_config.category_filter.AddExcludedCategory('*')
    chrome_trace_config.category_filter.AddIncludedCategory('blink.console')
    chrome_trace_config.category_filter.AddDisabledByDefault(
        'disabled-by-default-memory-infra')

  def GetExtraTracingMetrics(self):
    return super(TabSearchStoryMeasureMemory,
                 self).GetExtraTracingMetrics() + ['memoryMetric']


class TabSearchStoryMeasureMemoryBefore(TabSearchStoryMeasureMemory):
  NAME = 'tab_search:measure_memory:before'

  def RunNavigateSteps(self, action_runner):
    super(TabSearchStoryMeasureMemoryBefore,
          self).RunNavigateSteps(action_runner)
    action_runner.MeasureMemory(deterministic_mode=True)

  def InteractWithPage(self, action_runner):
    action_runner.Wait(1)


class TabSearchStoryMeasureMemoryAfter(TabSearchStoryMeasureMemory):
  NAME = 'tab_search:measure_memory:after'

  def InteractWithPage(self, action_runner):
    action_runner.MeasureMemory(deterministic_mode=True)


class TabSearchStoryMeasureMemoryMultiwindow(TabSearchStoryMeasureMemory):
  NAME = 'tab_search:measure_memory:multiwindow'

  def InteractWithPage(self, action_runner):
    action_runner.Wait(2)
    tabs = action_runner.tab.browser.tabs
    tabs.New(in_new_window=True)
    self.ToggleTabSearch(
        index=1)  # Toggle the tab search button in the 2nd window.
    action_runner.Wait(2)
    action_runner.MeasureMemory(deterministic_mode=True)


class TabSearchStoryMeasureMemory2TabSearch(TabSearchStoryMeasureMemory):
  NAME = 'tab_search:measure_memory:2tab_search'

  def RunNavigateSteps(self, action_runner):
    super(TabSearchStoryMeasureMemory2TabSearch,
          self).RunNavigateSteps(action_runner)
    tabs = action_runner.tab.browser.tabs
    new_tab = tabs.New()
    new_tab.Navigate(TAB_SEARCH_URL)
    new_tab.WaitForDocumentReadyStateToBeComplete()
    new_tab.action_runner.ExecuteJavaScript(MEASURE_JS_MEMORY %
                                            'tab_search:used_js_heap_size2')

  def InteractWithPage(self, action_runner):
    action_runner.MeasureMemory(deterministic_mode=True)


class TabSearchStoryMeasureMemory3TabSearch(TabSearchStoryMeasureMemory):
  NAME = 'tab_search:measure_memory:3tab_search'

  def RunNavigateSteps(self, action_runner):
    super(TabSearchStoryMeasureMemory3TabSearch,
          self).RunNavigateSteps(action_runner)
    tabs = action_runner.tab.browser.tabs
    for i in range(2):
      new_tab = tabs.New()
      new_tab.Navigate(TAB_SEARCH_URL)
      new_tab.WaitForDocumentReadyStateToBeComplete()
      new_tab.action_runner.ExecuteJavaScript(
          MEASURE_JS_MEMORY % ('tab_search:used_js_heap_size' + str(i + 2)))

  def InteractWithPage(self, action_runner):
    action_runner.MeasureMemory(deterministic_mode=True)


SCROLL_ELEMENT_FUNCTION = '''
document.querySelector('tab-search-app').shadowRoot
        .querySelector('tab-search-page').shadowRoot
        .getElementById('tabsList')
'''
