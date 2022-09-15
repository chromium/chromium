# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets.desktop_ui.multitab_story import MultiTabStory

NEW_TAB_PAGE_BENCHMARK_UMA = [
    'NewTabPage.LoadTime.WebUINTP',
    'NewTabPage.MainUi.ShownTime',
    'NewTabPage.Modules.ShownTime',
]

NEW_TAB_PAGE_URL = 'chrome://new-tab-page'


class NewTabPageStory(MultiTabStory):
  """Base class for new tab page stories"""
  URL_LIST = [NEW_TAB_PAGE_URL]
  # URL must be set to an external link in order to trigger a WPR download.
  URL = 'https://google.com'
  WAIT_FOR_NETWORK_QUIESCENCE = True

  def WillStartTracing(self, chrome_trace_config):
    super(NewTabPageStory, self).WillStartTracing(chrome_trace_config)
    chrome_trace_config.EnableUMAHistograms(*NEW_TAB_PAGE_BENCHMARK_UMA)

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(10)


class NewTabPageStoryLoading(NewTabPageStory):
  NAME = 'new_tab_page:loading'
