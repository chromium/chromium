# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets.desktop_ui.multitab_story import MultiTabStory
from page_sets.desktop_ui.ui_devtools_utils import ClickOn
from page_sets.desktop_ui.ui_devtools_utils import \
    SHIFT_DOWN, PressKey
from page_sets.desktop_ui.browser_element_identifiers \
    import kSideSearchButtonElementId

SIDE_SEARCH_BENCHMARK_UMA = [
    'SideSearch.LoadCompletedTime',
    'SideSearch.LoadDocumentTime',
]

GOOGLE_SEARCH_URL = 'https://www.google.com/search?q=test'
CHROME_VERSION_URL = 'chrome://version/'


class SideSearchStory(MultiTabStory):
  """Base class for side panel stories"""
  URL_LIST = [GOOGLE_SEARCH_URL]
  URL = URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = True

  def WillStartTracing(self, chrome_trace_config):
    super(SideSearchStory, self).WillStartTracing(chrome_trace_config)
    chrome_trace_config.EnableUMAHistograms(*SIDE_SEARCH_BENCHMARK_UMA)

  def RunPageInteractions(self, action_runner):
    action_runner.tab.Navigate(CHROME_VERSION_URL)
    action_runner.Wait(1)
    assert action_runner.tab.url == CHROME_VERSION_URL

    # Open side search.
    ClickOn(self._devtools, element_id=kSideSearchButtonElementId)
    action_runner.Wait(1)

    self.InteractWithPage(action_runner)

  def InteractWithPage(self, action_runner):
    pass


class SideSearchStoryNavigation(SideSearchStory):
  NAME = 'side_search:navigation'

  def InteractWithPage(self, action_runner):
    # Locate the 1st Window.
    node_id = self._devtools.QueryNodes('<Window>')[0]
    # Tab to the bottom of Web Contents and enter.
    PressKey(self._devtools, node_id, 'Tab', SHIFT_DOWN)
    PressKey(self._devtools, node_id, 'Return')

    # Wait some time for the page to load.
    action_runner.Wait(3)
    # Make sure the current tab is navigated away.
    assert action_runner.tab.url != CHROME_VERSION_URL


class SideSearchStoryMeasureMemory(SideSearchStory):
  NAME = 'side_search:measure_memory'

  def WillStartTracing(self, chrome_trace_config):
    super(SideSearchStoryMeasureMemory,
          self).WillStartTracing(chrome_trace_config)
    chrome_trace_config.category_filter.AddExcludedCategory('*')
    chrome_trace_config.category_filter.AddIncludedCategory('blink.console')
    chrome_trace_config.category_filter.AddDisabledByDefault(
        'disabled-by-default-memory-infra')

  def GetExtraTracingMetrics(self):
    return super(SideSearchStoryMeasureMemory,
                 self).GetExtraTracingMetrics() + ['memoryMetric']

  def InteractWithPage(self, action_runner):
    action_runner.MeasureMemory(deterministic_mode=True)
