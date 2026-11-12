# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import py_utils

from page_sets.desktop_ui.custom_metric_utils import SetMetricNames
from page_sets.desktop_ui.js_utils import MEASURE_JS_MEMORY
from page_sets.desktop_ui.multitab_story import MultiTabStory
from page_sets.desktop_ui.ui_devtools_utils import (
  PressKey,
  PLATFORM_ACCELERATOR,
  SHIFT_DOWN,
)
from page_sets.desktop_ui.url_list import TOP_URL
from page_sets.desktop_ui.webui_utils import Inspect
from page_sets.desktop_ui import story_tags

ORGANIZER_PANEL_BENCHMARK_UMA = [
  'Tabs.Organizer.CloseAction',
  'Tabs.Organizer.NumTabsClosedPerInstance',
  'Tabs.Organizer.PageHandlerConstructionDelay',
  'Tabs.Organizer.PanelDisplayedDuration',
  'Tabs.Organizer.Section.OpenTabs.ExpandedStateOnOpen',
  'Tabs.Organizer.Section.OpenTabs.ShowAllStateOnOpen',
  'Tabs.Organizer.Section.RecentlyClosed.ExpandedStateOnOpen',
  'Tabs.Organizer.Section.RecentlyClosed.ShowAllStateOnOpen',
  'Tabs.Organizer.Section.TabGroups.ExpandedStateOnOpen',
  'Tabs.Organizer.Section.TabGroups.ShowAllStateOnOpen',
  'Tabs.Organizer.WebUI.IndexOfCloseTabInFilteredList',
  'Tabs.Organizer.WebUI.IndexOfCloseTabInUnfilteredList',
  'Tabs.Organizer.WebUI.LoadCompletedTime',
  'Tabs.Organizer.WebUI.LoadDocumentTime',
  'Tabs.Organizer.WebUI.OpenTabsDataReceived',
  'Tabs.Organizer.WebUI.TimeToCloseTabInFilteredList',
  'Tabs.Organizer.WebUI.TimeToCloseTabInUnfilteredList',
  'Tabs.TabSearch.WebUI.SearchAlgorithmDuration',
]

ORGANIZER_PANEL_CUSTOM_METRIC_NAMES = [
  'OrganizerPanelPageHandlerConstructionDelay',
  'TabSearchPageHandler:GetProfileTabs',
]

ORGANIZER_PANEL_URL = 'chrome://organizer-panel.top-chrome/'


class OrganizerPanelStory(MultiTabStory):
  """Base class for organizer panel stories."""

  def RunPageInteractions(self, action_runner):
    SetMetricNames(action_runner, ORGANIZER_PANEL_CUSTOM_METRIC_NAMES)
    self.OpenOrganizerPanel()
    action_runner = Inspect(action_runner.tab.browser, ORGANIZER_PANEL_URL)
    action_runner.ExecuteJavaScript(
      MEASURE_JS_MEMORY % 'organizer_panel:used_js_heap_size_begin'
    )
    self.InteractWithPage(action_runner)
    action_runner.ExecuteJavaScript(
      MEASURE_JS_MEMORY % 'organizer_panel:used_js_heap_size_end'
    )

  def OpenOrganizerPanel(self, index=0):
    # Use keyboard shortcut to open the organizer panel.
    # index is used to select which window to send the shortcut to.
    window_node_id = self._devtools.QueryNodes('<Window>')[index]
    PressKey(
      self._devtools, window_node_id, 'A', PLATFORM_ACCELERATOR | SHIFT_DOWN
    )

  def CloseOrganizerPanel(self, index=0):
    # Use escape key to close the organizer panel.
    # index is used to select which window to send the key to.
    window_node_id = self._devtools.QueryNodes('<Window>')[index]
    PressKey(self._devtools, window_node_id, 'Esc')

  def InteractWithPage(self, action_runner):
    self.ShowAllSections(action_runner)
    self.ScrollTabs(action_runner)
    self.SearchTabs(action_runner)
    self.CloseTab(action_runner)

  def ShowAllSections(self, action_runner):
    action_runner.ExecuteJavaScript(SHOW_ALL_SECTIONS_SCRIPT)
    action_runner.Wait(0.5)

  def SearchTabs(self, action_runner):
    action_runner.Wait(1)
    action_runner.EnterText('o')
    action_runner.Wait(2)
    action_runner.PressKey('Backspace')
    action_runner.Wait(1)

  def CloseTab(self, action_runner):
    action_runner.Wait(1)
    # Tab to the first section header, arrow down to the 2nd tab, and arrow
    # right to its close button.
    action_runner.PressKey('Tab')
    action_runner.PressKey('ArrowDown', repeat_count=2, repeat_delay_ms=200)
    action_runner.PressKey('ArrowRight')
    action_runner.PressKey(' ')
    action_runner.Wait(1)

  def ScrollTabs(self, action_runner):
    action_runner.Wait(1)
    self.StartMeasuringFrameTime(
      action_runner, 'organizer_panel:frame_time_on_scroll'
    )
    action_runner.ScrollElement(element_function=SCROLL_ELEMENT_FUNCTION)
    self.StopMeasuringFrameTime(action_runner)
    action_runner.Wait(1)

  def CloseAndOpen(self, action_runner):
    action_runner.Wait(1)
    self.CloseOrganizerPanel()
    action_runner.Wait(1)
    self.OpenOrganizerPanel()
    action_runner.Wait(5)

  def CloseAndOpenLoading(self, action_runner):
    action_runner.Wait(1)
    self.CloseOrganizerPanel()
    action_runner.Wait(1)
    tabs = action_runner.tab.browser.tabs
    for i, url in enumerate(self.URL_LIST2):
      tabs[i].Navigate(url)
    self.OpenOrganizerPanel()
    action_runner.Wait(5)

  def ScrollUpAndDown(self, action_runner):
    self.ShowAllSections(action_runner)
    action_runner.Wait(1)
    self.StartMeasuringFrameTime(
      action_runner, 'organizer_panel:frame_time_on_first_scroll_down'
    )
    action_runner.ScrollElement(element_function=SCROLL_ELEMENT_FUNCTION)
    self.StartMeasuringFrameTime(
      action_runner, 'organizer_panel:frame_time_on_first_scroll_up'
    )
    action_runner.ScrollElement(
      element_function=SCROLL_ELEMENT_FUNCTION, direction='up'
    )
    self.StartMeasuringFrameTime(
      action_runner, 'organizer_panel:frame_time_on_second_scroll_down'
    )
    action_runner.ScrollElement(element_function=SCROLL_ELEMENT_FUNCTION)
    self.StopMeasuringFrameTime(action_runner)
    action_runner.Wait(1)

  def WillStartTracing(self, chrome_trace_config):
    super(OrganizerPanelStory, self).WillStartTracing(chrome_trace_config)
    chrome_trace_config.EnableUMAHistograms(*ORGANIZER_PANEL_BENCHMARK_UMA)


class OrganizerPanelRecentlyClosedStory(OrganizerPanelStory):
  """Base class for organizer panel recently closed stories."""

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


class OrganizerPanelStoryRecentlyClosed10(OrganizerPanelRecentlyClosedStory):
  NAME = 'organizer_panel:10_recently_closed'
  URL_LIST = ['chrome://version?q={}'.format(i) for i in range(10)]
  URL = URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = True


class OrganizerPanelStoryRecentlyClosed50(OrganizerPanelRecentlyClosedStory):
  NAME = 'organizer_panel:50_recently_closed'
  URL_LIST = ['chrome://version?q={}'.format(i) for i in range(50)]
  URL = URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = True


class OrganizerPanelStoryRecentlyClosed100(OrganizerPanelRecentlyClosedStory):
  NAME = 'organizer_panel:100_recently_closed'
  URL_LIST = ['chrome://version?q={}'.format(i) for i in range(100)]
  URL = URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = True


class OrganizerPanelStoryTop10(OrganizerPanelStory):
  NAME = 'organizer_panel:top10:2020'
  URL_LIST = TOP_URL[:10]
  URL = URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = True


class OrganizerPanelStoryTop50(OrganizerPanelStory):
  NAME = 'organizer_panel:top50:2020'
  URL_LIST = TOP_URL[:50]
  URL = URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = True


class OrganizerPanelStoryTop10Loading(OrganizerPanelStory):
  NAME = 'organizer_panel:top10:loading:2020'
  URL_LIST = TOP_URL[:10]
  URL = URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = False


class OrganizerPanelStoryTop50Loading(OrganizerPanelStory):
  NAME = 'organizer_panel:top50:loading:2020'
  URL_LIST = TOP_URL[:50]
  URL = URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = False


class OrganizerPanelStoryCloseAndOpen(OrganizerPanelStory):
  NAME = 'organizer_panel:close_and_open:2020'
  URL_LIST = TOP_URL[:10]
  URL = URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = True

  def InteractWithPage(self, action_runner):
    self.CloseAndOpen(action_runner)


class OrganizerPanelStoryCloseAndOpenLoading(OrganizerPanelStory):
  NAME = 'organizer_panel:close_and_open:loading:2020'
  URL_LIST = TOP_URL[:10]
  URL_LIST2 = TOP_URL[10:20]
  URL = URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = False

  def InteractWithPage(self, action_runner):
    self.CloseAndOpenLoading(action_runner)


class OrganizerPanelStoryScrollUpAndDown(OrganizerPanelStory):
  NAME = 'organizer_panel:scroll_up_and_down:2020'
  URL_LIST = TOP_URL[:50]
  URL = URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = False

  def InteractWithPage(self, action_runner):
    self.ScrollUpAndDown(action_runner)


class OrganizerPanelStoryCleanSlate(OrganizerPanelStory):
  NAME = 'organizer_panel:clean_slate'
  URL_LIST = []
  URL = 'about:blank'
  WAIT_FOR_NETWORK_QUIESCENCE = False
  TAGS = [story_tags.SMOKE_TEST]

  def InteractWithPage(self, action_runner):
    action_runner.Wait(1)


class OrganizerPanelStoryMeasureMemory(OrganizerPanelStory):
  URL_LIST = []
  URL = 'about:blank'
  WAIT_FOR_NETWORK_QUIESCENCE = False

  def WillStartTracing(self, chrome_trace_config):
    super(OrganizerPanelStoryMeasureMemory, self).WillStartTracing(
      chrome_trace_config
    )
    chrome_trace_config.category_filter.AddExcludedCategory('*')
    chrome_trace_config.category_filter.AddIncludedCategory('blink.console')
    chrome_trace_config.category_filter.AddDisabledByDefault(
      'disabled-by-default-memory-infra'
    )

  def GetExtraTracingMetrics(self):
    return super(
      OrganizerPanelStoryMeasureMemory, self
    ).GetExtraTracingMetrics() + ['memoryMetric']


class OrganizerPanelStoryMeasureMemoryBefore(OrganizerPanelStoryMeasureMemory):
  NAME = 'organizer_panel:measure_memory:before'

  def RunNavigateSteps(self, action_runner):
    super(OrganizerPanelStoryMeasureMemoryBefore, self).RunNavigateSteps(
      action_runner
    )
    action_runner.MeasureMemory(deterministic_mode=True)

  def InteractWithPage(self, action_runner):
    action_runner.Wait(1)


class OrganizerPanelStoryMeasureMemoryAfter(OrganizerPanelStoryMeasureMemory):
  NAME = 'organizer_panel:measure_memory:after'

  def InteractWithPage(self, action_runner):
    # MeasureMemory(deterministic_mode=True) calls CollectGarbage(), which
    # requires a 'page' DevTools target (InspectorBackend._page). Because the
    # organizer panel WebContents is not a 'page' target when inspected, call
    # MeasureMemory on tabs[0]; DumpMemory() still captures all processes.
    action_runner.tab.browser.tabs[0].action_runner.MeasureMemory(
      deterministic_mode=True
    )


class OrganizerPanelStoryMeasureMemoryMultiwindow(
  OrganizerPanelStoryMeasureMemory
):
  NAME = 'organizer_panel:measure_memory:multiwindow'

  def InteractWithPage(self, action_runner):
    action_runner.Wait(2)
    tabs = action_runner.tab.browser.tabs
    tabs.New(in_new_window=True)
    self.OpenOrganizerPanel(
      index=1
    )  # Open the organizer panel in the 2nd window.
    action_runner.Wait(2)
    # See OrganizerPanelStoryMeasureMemoryAfter for why tabs[0] is used.
    tabs[0].action_runner.MeasureMemory(deterministic_mode=True)


class OrganizerPanelStoryMeasureMemory2OrganizerPanel(
  OrganizerPanelStoryMeasureMemory
):
  NAME = 'organizer_panel:measure_memory:2organizer_panel'

  def RunNavigateSteps(self, action_runner):
    super(
      OrganizerPanelStoryMeasureMemory2OrganizerPanel, self
    ).RunNavigateSteps(action_runner)
    tabs = action_runner.tab.browser.tabs
    new_tab = tabs.New()
    new_tab.Navigate(ORGANIZER_PANEL_URL)
    new_tab.WaitForDocumentReadyStateToBeComplete()
    new_tab.action_runner.ExecuteJavaScript(
      MEASURE_JS_MEMORY % 'organizer_panel:used_js_heap_size2'
    )

  def InteractWithPage(self, action_runner):
    # See OrganizerPanelStoryMeasureMemoryAfter for why tabs[0] is used.
    action_runner.tab.browser.tabs[0].action_runner.MeasureMemory(
      deterministic_mode=True
    )


class OrganizerPanelStoryMeasureMemory3OrganizerPanel(
  OrganizerPanelStoryMeasureMemory
):
  NAME = 'organizer_panel:measure_memory:3organizer_panel'

  def RunNavigateSteps(self, action_runner):
    super(
      OrganizerPanelStoryMeasureMemory3OrganizerPanel, self
    ).RunNavigateSteps(action_runner)
    tabs = action_runner.tab.browser.tabs
    for i in range(2):
      new_tab = tabs.New()
      new_tab.Navigate(ORGANIZER_PANEL_URL)
      new_tab.WaitForDocumentReadyStateToBeComplete()
      new_tab.action_runner.ExecuteJavaScript(
        MEASURE_JS_MEMORY % ('organizer_panel:used_js_heap_size' + str(i + 2))
      )

  def InteractWithPage(self, action_runner):
    # See OrganizerPanelStoryMeasureMemoryAfter for why tabs[0] is used.
    action_runner.tab.browser.tabs[0].action_runner.MeasureMemory(
      deterministic_mode=True
    )


SCROLL_ELEMENT_FUNCTION = '''
document.querySelector('organizer-panel-app').shadowRoot
        .getElementById('list')
'''

# Clicks the real section menu to show all items, which also records
# Tabs.Organizer.Section.*.MenuAction and persists show-all via
# setSectionShowAll (isolated per story since DesktopUISharedState restarts
# the browser for each run).
SHOW_ALL_SECTIONS_SCRIPT = '''
(() => {
  const app = document.querySelector('organizer-panel-app');
  const list = app.shadowRoot.getElementById('list');
  const sections = list.shadowRoot.querySelectorAll('organizer-list-section');
  for (const section of sections) {
    const header = section.shadowRoot.getElementById('header');
    if (header && !header.showAll) {
      header.shadowRoot.getElementById('menuButton').click();
      header.shadowRoot.getElementById('showAllButton').click();
    }
  }
  app.$.searchField.showAndFocus();
})();
'''
