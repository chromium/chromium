# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import logging
import time

import py_utils
from telemetry.core import exceptions
from telemetry.page import page as page_module
from telemetry.page import shared_page_state
from contrib.cros_benchmarks import cros_utils


# NOTE: When tab count is high, some tabs may be discarded, and the tab
# context would be invalidated. Avoid storing tab object for later use.
class CrosMultiTabStory(page_module.Page):
  """Base class for multi-tab stories."""

  def __init__(self, story_set, cros_remote, tabset_repeat=1,
               pause_after_creation=0, pause_after_switch=3):
    super(CrosMultiTabStory, self).__init__(
        shared_page_state_class=shared_page_state.SharedPageState,
        page_set=story_set, name=self.NAME, url=self.URL)
    # cros_remote is the DUT IP or None if running locally.
    self._cros_remote = cros_remote
    self._tabset_repeat = tabset_repeat
    self._pause_after_creation = pause_after_creation
    self._pause_after_switch = pause_after_switch

  def RunNavigateSteps(self, action_runner):
    """Opening tabs and waiting for them to load."""
    if not self._cros_remote and not py_utils.IsRunningOnCrosDevice():
      raise ValueError('Must specify --remote=DUT_IP to run this test, '
                       'or run it on CrOS locally.')

    # As this story may run for a long time, adjusting screen off time to
    # avoid screen off.
    cros_utils.NoScreenOff(self._cros_remote)

    tabs = action_runner.tab.browser.tabs

    # No need to create the first tab as there is already one
    # when the browser is ready.
    url_list = self.URL_LIST * self._tabset_repeat
    if url_list:
      action_runner.Navigate(url_list[0])
    for i, url in enumerate(url_list[1:]):
      new_tab = tabs.New()
      try:
        new_tab.action_runner.Navigate(url)
      except exceptions.DevtoolsTargetCrashException:
        logging.info('Navigate: devtools context lost')
      if i % 10 == 0:
        print('opening tab:', i)

    # Waiting for every tabs to be stable.
    for i, url in enumerate(url_list):
      try:
        tabs[i].action_runner.WaitForNetworkQuiescence()
      except py_utils.TimeoutException:
        logging.info('WaitForNetworkQuiescence() timeout, url[%d]: %s',
                     i, url)
      except exceptions.DevtoolsTargetCrashException:
        logging.info('WaitForNetworkQuiescence: devtools context lost')

  def RunPageInteractions(self, action_runner):
    """Tab switching to each tabs."""
    url_list = self.URL_LIST * self._tabset_repeat
    browser = action_runner.tab.browser
    total_tab_count = len(url_list)
    live_tab_count = len(browser.tabs)
    if live_tab_count != total_tab_count:
      logging.warning('live tab: %d, tab discarded: %d',
                      live_tab_count, total_tab_count - live_tab_count)

    time.sleep(self._pause_after_creation)

    with cros_utils.KeyboardEmulator(self._cros_remote) as keyboard:
      for i in range(total_tab_count):
        keyboard.SwitchTab()
        time.sleep(self._pause_after_switch)

        if i % 10 == 0:
          print('switching tab:', i)


class CrosMultiTabTypical24Story(CrosMultiTabStory):
  """Multi-tab stories to test 24 typical webpages."""
  NAME = 'cros_tab_switching_typical24'
  URL_LIST = [
      # Why: Alexa games #48
      'http://www.nick.com/games',
      # Why: Alexa sports #45
      'http://www.rei.com/',
      # Why: Alexa sports #50
      'http://www.fifa.com/',
      # Why: Alexa shopping #41
      'http://www.gamestop.com/ps3',
      # Why: Alexa news #55
      ('http://www.economist.com/news/science-and-technology/21573529-small-'
       'models-cosmic-phenomena-are-shedding-light-real-thing-how-build'),
      # Why: Alexa news #67
      'http://www.theonion.com',
      'http://arstechnica.com/',
      # Why: Alexa home #10
      'http://allrecipes.com/Recipe/Pull-Apart-Hot-Cross-Buns/Detail.aspx',
      'http://www.html5rocks.com/en/',
      'http://www.mlb.com/',
      ('http://gawker.com/5939683/based-on-a-true-story-is-a-rotten-lie-i-'
       'hope-you-never-believe'),
      'http://www.imdb.com/title/tt0910970/',
      'http://www.flickr.com/search/?q=monkeys&f=hp',
      'http://money.cnn.com/',
      'http://www.nationalgeographic.com/',
      'http://premierleague.com',
      'http://www.osubeavers.com/',
      'http://walgreens.com',
      'http://colorado.edu',
      ('http://www.ticketmaster.com/JAY-Z-and-Justin-Timberlake-tickets/artist/'
       '1837448?brand=none&tm_link=tm_homeA_rc_name2'),
      # pylint: disable=line-too-long
      'http://www.theverge.com/2013/3/5/4061684/inside-ted-the-smartest-bubble-in-the-world',
      'http://www.airbnb.com/',
      'http://www.ign.com/',
      # Why: Alexa health #25
      'http://www.fda.gov',
  ]
  URL = URL_LIST[0]
