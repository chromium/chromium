# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
from page_sets.desktop_ui.browser_utils import Resize
from page_sets.desktop_ui.js_utils import MEASURE_JS_MEMORY
from page_sets.desktop_ui.multitab_story import MultiTabStory
from page_sets.desktop_ui.ui_devtools_utils import ClickOn, IsMac, PressKey
from page_sets.desktop_ui.url_list import TOP_URL
from page_sets.desktop_ui.webui_utils import Inspect

from telemetry.internal.browser.ui_devtools import MOUSE_EVENT_BUTTON_RIGHT

DOWNLOAD_SHELF_BENCHMARK_UMA = [
    'Download.Shelf.Views.FirstDownloadPaintTime',
    'Download.Shelf.Views.NotFirstDownloadPaintTime',
    'Download.Shelf.Views.ShowContextMenuTime',
    'Download.Shelf.WebUI.FirstDownloadPaintTime',
    'Download.Shelf.WebUI.LoadCompletedTime',
    'Download.Shelf.WebUI.LoadDocumentTime',
    'Download.Shelf.WebUI.NotFirstDownloadPaintTime',
    'Download.Shelf.WebUI.ShowContextMenuTime',
]

DOWNLOAD_URL = 'https://dl.google.com/chrome/mac/stable/GGRO/googlechrome.dmg'
WEBUI_DOWNLOAD_SHELF_URL = 'chrome://download-shelf.top-chrome/'


class DownloadShelfStory(MultiTabStory):
  """Base class for stories to download files"""

  def RunNavigateSteps(self, action_runner):
    url_list = self.URL_LIST
    tabs = action_runner.tab.browser.tabs
    for url in url_list:
      # Suppress error caused by tab closed before it returns occasionally
      try:
        tabs.New(url=url)
      except Exception:
        pass
    if not IsMac():
      self._devtools = action_runner.tab.browser.GetUIDevtools()

  def IsWebUI(self):
    return 'webui' in self.NAME

  def RunPageInteractions(self, action_runner):
    # Wait for download items to show up, this may take quite some time
    # for lowend machines.
    action_runner.Wait(10)
    if self.IsWebUI():
      action_runner = Inspect(action_runner.tab.browser,
                              WEBUI_DOWNLOAD_SHELF_URL)
      action_runner.ExecuteJavaScript(MEASURE_JS_MEMORY %
                                      'download_shelf:used_js_heap_size_begin')
    self.InteractWithPage(action_runner)
    if self.IsWebUI():
      action_runner.ExecuteJavaScript(MEASURE_JS_MEMORY %
                                      'download_shelf:used_js_heap_size_end')

  def InteractWithPage(self, action_runner):
    self.ContextMenu(action_runner)
    action_runner.Wait(2)
    browser = action_runner.tab.browser
    Resize(browser, browser.tabs[0].id, start_width=600, end_width=800)
    action_runner.Wait(2)

  def ContextMenu(self, action_runner):
    if IsMac():
      return
    try:
      if self.IsWebUI():
        action_runner.ClickElement(
            element_function=DROPDOWN_BUTTON_ELEMENT_FUNCTION)
      else:
        ClickOn(self._devtools,
                'TransparentButton',
                button=MOUSE_EVENT_BUTTON_RIGHT)
      action_runner.Wait(1)
      node_id = self._devtools.QueryNodes('<Window>')[
          -1]  # Context menu lives in the last Window.
      PressKey(self._devtools, node_id, 'Esc')
      action_runner.Wait(1)
    except Exception as e:
      logging.warning('Failed to run context menu. Error: %s', e)

  def WillStartTracing(self, chrome_trace_config):
    super(DownloadShelfStory, self).WillStartTracing(chrome_trace_config)
    chrome_trace_config.EnableUMAHistograms(*DOWNLOAD_SHELF_BENCHMARK_UMA)


class DownloadShelfStory1File(DownloadShelfStory):
  NAME = 'download_shelf:1file'
  URL_LIST = [DOWNLOAD_URL]
  URL = URL_LIST[0]


class DownloadShelfStory5File(DownloadShelfStory):
  NAME = 'download_shelf:5file'
  URL_LIST = [DOWNLOAD_URL] * 5
  URL = URL_LIST[0]


class DownloadShelfStoryTop10Loading(DownloadShelfStory):
  NAME = 'download_shelf:top10:loading'
  URL_LIST = TOP_URL[:10] + [DOWNLOAD_URL]
  URL = URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = False


class DownloadShelfStoryMeasureMemory(DownloadShelfStory):
  NAME = 'download_shelf:measure_memory'
  URL_LIST = [DOWNLOAD_URL]
  URL = URL_LIST[0]

  def WillStartTracing(self, chrome_trace_config):
    super(DownloadShelfStoryMeasureMemory,
          self).WillStartTracing(chrome_trace_config)
    chrome_trace_config.category_filter.AddExcludedCategory('*')
    chrome_trace_config.category_filter.AddIncludedCategory('blink.console')
    chrome_trace_config.category_filter.AddDisabledByDefault(
        'disabled-by-default-memory-infra')

  def GetExtraTracingMetrics(self):
    return super(DownloadShelfStoryMeasureMemory,
                 self).GetExtraTracingMetrics() + ['memoryMetric']

  def InteractWithPage(self, action_runner):
    action_runner.MeasureMemory(deterministic_mode=True)


class DownloadShelfWebUIStory1File(DownloadShelfStory):
  NAME = 'download_shelf_webui:1file'
  URL_LIST = [DOWNLOAD_URL]
  URL = URL_LIST[0]


class DownloadShelfWebUIStory5File(DownloadShelfStory):
  NAME = 'download_shelf_webui:5file'
  URL_LIST = [DOWNLOAD_URL] * 5
  URL = URL_LIST[0]


class DownloadShelfWebUIStoryTop10Loading(DownloadShelfStory):
  NAME = 'download_shelf_webui:top10:loading'
  URL_LIST = TOP_URL[:10] + [DOWNLOAD_URL]
  URL = URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = False


class DownloadShelfWebUIStoryMeasureMemory(DownloadShelfStoryMeasureMemory):
  NAME = 'download_shelf_webui:measure_memory'
  URL_LIST = [DOWNLOAD_URL]
  URL = URL_LIST[0]


DROPDOWN_BUTTON_ELEMENT_FUNCTION = '''
document.querySelector('download-shelf-app').shadowRoot.
querySelector('download-list').shadowRoot.
querySelector('download-item').shadowRoot.
getElementById('dropdown-button')
'''
