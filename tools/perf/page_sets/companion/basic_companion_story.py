# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
from page_sets.desktop_ui.multitab_story import MultiTabStory
from page_sets.desktop_ui.ui_devtools_utils import ClickOn
from page_sets.companion.browser_element_identifiers import kSidePanelButtonElementId

GOOGLE_URL = "https://www.google.com/"
TEST_SEARCH_URL = "https://www.google.com/search?q=test"
BLANK_URL = "about:blank"
STARTUP_HISTOGRAM = "Startup.BrowserMainRunnerImplInitializeLongTime"


class CompanionStory(MultiTabStory):
  """Base class for companion stories"""
  URL_LIST = [TEST_SEARCH_URL]
  URL = URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = True

  def RunPageInteractions(self, action_runner):
    action_runner.tab.Navigate(BLANK_URL)
    action_runner.Wait(1)
    assert action_runner.tab.url == BLANK_URL
    self.InteractWithPage(action_runner)

  def ToggleSidePanel(self, action_runner):
    ClickOn(self._devtools, element_id=kSidePanelButtonElementId)
    action_runner.Wait(1)

  def SanityHistogramCheck(self, action_runner):
    action_runner.tab.Navigate(GOOGLE_URL)
    action_runner.Wait(1)
    assert action_runner.tab.url == GOOGLE_URL, "%s" % action_runner.tab.url
    histogram = self.FetchHistogram(action_runner, STARTUP_HISTOGRAM)
    assert histogram["count"] == 1

  def FetchHistogram(self, action_runner, name):
    js = "statsCollectionController.getBrowserHistogram('%s');" % name
    return json.loads(action_runner.EvaluateJavaScript(js))

  def InteractWithPage(self, action_runner):
    pass


class CompanionStoryBasic(CompanionStory):
  NAME = 'companion:basic_startup'

  def InteractWithPage(self, action_runner):
    self.SanityHistogramCheck(action_runner)
