# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging

from telemetry.page import page as page_module
from telemetry.page import shared_page_state
from telemetry import story

# TODO(crbug.com/944954): Move story_tags to a location outside system_health.
from page_sets.system_health import story_tags


class _SharedPageState(shared_page_state.SharedDesktopPageState):

  def CanRunOnBrowser(self, browser_info, page):
    if not hasattr(page, 'CanRunOnBrowser'):
      return True
    return page.CanRunOnBrowser(browser_info.browser)


class DesktopUIPage(page_module.Page):

  def __init__(self, url, page_set, name, extra_browser_args=None, tags=None):
    tags = tags or []
    super(DesktopUIPage, self).__init__(
        url=url,
        page_set=page_set,
        name=name,
        shared_page_state_class=_SharedPageState,
        extra_browser_args=extra_browser_args,
        tags=tags)


class OverviewMode(DesktopUIPage):

  def CanRunOnBrowser(self, browser):
    return browser.supports_overview_mode

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(1)
    # TODO(chiniforooshan): CreateInteraction creates an async event in the
    # renderer, which works fine; it is nicer if we create UI interaction
    # records in the browser process.
    with action_runner.CreateInteraction('ui_EnterOverviewAction'):
      action_runner.EnterOverviewMode()
      # TODO(chiniforooshan): The following wait, and the one after
      # ExitOverviewMode(), is a workaround for crbug.com/788454. Remove when
      # the bug is fixed.
      action_runner.Wait(1)
    action_runner.Wait(0.5)
    with action_runner.CreateInteraction('ui_ExitOverviewAction'):
      action_runner.ExitOverviewMode()
      action_runner.Wait(1)


class MultiWindowOverviewMode(OverviewMode):

  def __init__(self, url_list, page_set, name, tags=None):
    self.url_list = url_list
    tags = [tag.name for tag in tags] if tags else []
    tags.append("multiwindow")
    super(MultiWindowOverviewMode, self).__init__(
      url=self.url_list[0],
      page_set=page_set,
      name=name,
      extra_browser_args=["--disable-popup-blocking"],
      tags=tags)

  def RunNavigateSteps(self, action_runner):
    if self.url_list:
      action_runner.Navigate(self.url)

    tabs = action_runner.tab.browser.tabs
    for i, url in enumerate(self.url_list[1:]):
      new_window = tabs.New(in_new_window=True)
      new_window.action_runner.Navigate(url)
      logging.info('Navigate: opened window #%d', i + 2)


class CrosUiCasesPageSet(story.StorySet):
  """Pages that test desktop UI performance."""

  def __init__(self):
    super(CrosUiCasesPageSet, self).__init__(
      archive_data_file='data/cros_ui_cases.json',
      cloud_storage_bucket=story.PARTNER_BUCKET)

    self.AddStory(OverviewMode(
        'http://news.yahoo.com', self, 'overview:yahoo_news'))
    self.AddStory(OverviewMode(
        'http://jsbin.com/giqafofe/1/quiet?JS_POSTER_CIRCLE', self,
        'overview:js_poster_circle'))
    self.AddStory(MultiWindowOverviewMode(
      ["about:blank",
       "about:blank",
       "about:blank",
       "about:blank",
       "about:blank"], self, 'overview:multiwindow_five_blank_pages'))
    self.AddStory(MultiWindowOverviewMode(
      ["https://www.seriouseats.com",
       "https://www.theroot.com/",
       "https://www.aljazeera.com/",
       "https://www.youtube.com/watch?v=RjsLm5PCdVQ",
       "https://www.tmall.com/"], self,
       'overview:multiwindow_five_real_pages:2019',
       tags=[story_tags.YEAR_2019, story_tags.INTERNATIONAL]))
    self.AddStory(MultiWindowOverviewMode(
      [ # "https://en.wikipedia.org/wiki/Maryam_Mirzakhani",  # crbug.com/944604
       "https://unsplash.com/search/photos/kitten",
       "https://en.wikipedia.org/wiki/Maryam_Mirzakhani",
       "http://www.unwomen.org/en/csw",
       "https://www.who.int/hiv/en/",
       "https://www.weibo.com/login.php",
       ("https://www.harpersbazaararabia.com/fashion/runway/"
        "arab-fashion-week-runway-2018"),
       "https://www.nrdc.org/flint",
       "https://en.wikipedia.org/wiki/Brouwer_fixed-point_theorem",
       ("http://theundefeated.com/features/simone-biles-most-"
        "dominant-athlete-of-2018/"),
       "https://www.factcheck.org/about/our-mission/"], self,
       'overview:multiwindow_ten_real_pages:2019',
       tags=[story_tags.YEAR_2019, story_tags.INTERNATIONAL]))
