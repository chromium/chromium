# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import subprocess
from page_sets.desktop_ui.multitab_story import MultiTabStory
from page_sets.desktop_ui.ui_devtools_utils import ClickOn, InputText, \
    PressKey, SHIFT_DOWN
from page_sets.companion.browser_element_identifiers import \
    kToolbarSidePanelButtonElementId, kSidePanelComboboxElementId
from page_sets.companion.histograms import _CQ, _PROMO_EVENT, _SEARCH_BOX, \
    _STARTUP, _ZERO_STATE
from page_sets.login_helpers import google_login
from page_sets.data.companion_test_sites import SITES

SCREENSHOT_PATH = "/tmp/"

_GOOGLE_URL = "https://www.google.com/"
_TEST_SEARCH_URL = "https://www.google.com/search?q=test"
_TEST_PAGE_URL = "https://www.westelm.com"
_BLANK_URL = "about:blank"
_COMPANION_URL = "https://lens.google.com/companion?pli=1"


class CompanionStory(MultiTabStory):
  """Base class for companion stories"""
  URL_LIST = [_TEST_SEARCH_URL]
  URL = URL_LIST[0]
  WAIT_FOR_NETWORK_QUIESCENCE = True

  def RunPageInteractions(self, action_runner):
    action_runner.tab.Navigate(_BLANK_URL)
    action_runner.Wait(1)
    assert action_runner.tab.url == _BLANK_URL
    self.InteractWithPage(action_runner)

  def ToggleSidePanel(self, action_runner):
    ClickOn(self._devtools, element_id=kToolbarSidePanelButtonElementId)
    action_runner.Wait(3)

  def OpenCompanion(self, action_runner):
    self.ToggleSidePanel(action_runner)
    combobox_node_id = self._devtools.QueryNodes("id:%s" %
                                                 kSidePanelComboboxElementId)[0]
    PressKey(self._devtools, combobox_node_id, " ")
    action_runner.Wait(1)
    window_node_id = self._devtools.QueryNodes("<Window>")[0]
    PressKey(self._devtools, window_node_id, "ArrowUp")
    PressKey(self._devtools, window_node_id, "Return")

  def LogIn(self, action_runner):
    google_login.NewLoginGoogleAccount(action_runner, "intelligence")

  def SanityHistogramCheck(self, action_runner):
    action_runner.tab.Navigate(_GOOGLE_URL)
    action_runner.Wait(1)
    assert action_runner.tab.url == _GOOGLE_URL, "%s" % action_runner.tab.url
    histogram = self.FetchHistogram(action_runner, _STARTUP)
    assert histogram["count"] == 1

  def ConductHistogramCheck(self,
                            action_runner,
                            histogram_name,
                            expected_empty=False,
                            expected_count=None):
    histogram = self.FetchHistogram(action_runner, histogram_name)
    if expected_empty:
      assert not histogram, "expected non-existent %s histogram, got %s" % \
          (histogram_name, histogram)
    else:
      assert histogram, "%s histogram is non-existent" % histogram_name

    if expected_count:
      assert histogram['count'] == expected_count, \
          "Expected %s, got %s for histogram %s" % (expected_count,
                                                     histogram['count'],
                                                     histogram_name)

  def FetchHistogram(self, action_runner, name):
    js = "statsCollectionController.getBrowserHistogram('%s');" % name
    return json.loads(action_runner.EvaluateJavaScript(js))

  def InteractWithPage(self, action_runner):
    pass


class CompanionStoryBasicOpen(CompanionStory):
  NAME = 'companion:basic_open_logged_in'

  def InteractWithPage(self, action_runner):
    self.SanityHistogramCheck(action_runner)
    self.LogIn(action_runner)
    action_runner.Wait(1)
    action_runner.tab.Navigate(_TEST_PAGE_URL)
    action_runner.Wait(3)

    # Open companion and ensure features loaded.
    self.OpenCompanion(action_runner)
    action_runner.Wait(10)
    self.ConductHistogramCheck(action_runner, _ZERO_STATE)
    self.ConductHistogramCheck(action_runner, _CQ)


class CompanionStoryBasicOpenLoggedOut(CompanionStory):
  NAME = 'companion:basic_open_logged_out'

  def InteractWithPage(self, action_runner):
    self.SanityHistogramCheck(action_runner)
    action_runner.tab.Navigate(_TEST_PAGE_URL)
    action_runner.Wait(3)

    # Open companion and ensure features loaded.
    self.OpenCompanion(action_runner)
    action_runner.Wait(10)
    self.ConductHistogramCheck(action_runner, _ZERO_STATE)
    self.ConductHistogramCheck(action_runner, _PROMO_EVENT)
    self.ConductHistogramCheck(action_runner, _CQ, expected_empty=True)


class CompanionStorySRP(CompanionStory):
  NAME = 'companion:srp'

  def ConductSideSearch(self, action_runner):
    action_runner.Wait(2)
    node_id = self._devtools.QueryNodes('<Window>')[0]
    action_runner.Wait(2)
    PressKey(self._devtools, node_id, 'Tab', SHIFT_DOWN)
    PressKey(self._devtools, node_id, 'Tab', SHIFT_DOWN)
    PressKey(self._devtools, node_id, 'Tab', SHIFT_DOWN)
    InputText(self._devtools, node_id, 'test input')
    action_runner.Wait(1)
    PressKey(self._devtools, node_id, 'Tab', SHIFT_DOWN)
    PressKey(self._devtools, node_id, ' ')
    PressKey(self._devtools, node_id, ' ')
    action_runner.PressKey(' ')
    action_runner.PressKey(' ')

  def InteractWithPage(self, action_runner):
    self.SanityHistogramCheck(action_runner)
    self.LogIn(action_runner)
    action_runner.Wait(1)
    action_runner.tab.Navigate(_TEST_PAGE_URL)
    action_runner.Wait(3)

    # Open companion and ensure features loaded.
    self.OpenCompanion(action_runner)
    action_runner.Wait(10)
    self.ConductHistogramCheck(action_runner, _ZERO_STATE)
    self.ConductHistogramCheck(action_runner, _PROMO_EVENT)
    self.ConductHistogramCheck(action_runner, _CQ)

    # Conduct Side Search
    self.ConductHistogramCheck(action_runner, _SEARCH_BOX, expected_empty=True)
    self.ConductSideSearch(action_runner)
    self.ConductHistogramCheck(action_runner, _SEARCH_BOX)


class CompanionStoryScreenshot(CompanionStory):
  NAME = 'companion:screenshot'

  def InteractWithPage(self, action_runner):
    self.SanityHistogramCheck(action_runner)
    self.LogIn(action_runner)
    action_runner.Wait(5)
    self.OpenCompanion(action_runner)

    for count, site in enumerate(SITES):
      action_runner.tab.Navigate(site)
      action_runner.Wait(15)
      subprocess.call(["scrot", SCREENSHOT_PATH + str(count) + ".png"])
