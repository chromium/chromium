# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import inspect
import os

from telemetry.page.page import Page
from telemetry.story.story_set import StorySet
from telemetry.page.shared_page_state import SharedPageState
import py_utils


class _GenericPage(object):
  def __init__(self,
               url,
               scroll_repeat_count=6,
               accept_cookies_button_selector=None):
    self._url = url
    self._scroll_repeat_count = scroll_repeat_count
    self._accept_cookies_button_selector = accept_cookies_button_selector

  @property
  def url(self):
    return self._url

  def RunNavigateSteps(self, action_runner):
    action_runner.Navigate(self._url)
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()
    action_runner.Wait(5)

  def RunPageInteractions(self, action_runner):
    self.AllowCookies(action_runner)
    action_runner.Wait(2)
    self.Scroll(action_runner)

  def Scroll(self, action_runner):
    for _ in range(self._scroll_repeat_count):
      action_runner.ScrollPage(distance=800, use_touch=True)
      action_runner.Wait(0.4)

  def AllowCookies(self, action_runner):
    if self._accept_cookies_button_selector is not None:
      self.MaybeTapElement(action_runner,
                           selector=self._accept_cookies_button_selector)

  def IsElementPresent(self, action_runner, text=None, selector=None):
    try:
      action_runner.WaitForElement(text=text,
                                   selector=selector,
                                   timeout_in_seconds=5)
    except py_utils.TimeoutException:
      return False
    return True

  def MaybeTapElement(self, action_runner, text=None, selector=None):
    if self.IsElementPresent(action_runner, text=text, selector=selector):
      action_runner.TapElement(text=text, selector=selector)


class _AmazonPage(_GenericPage):
  def __init__(self):
    super(_AmazonPage, self).__init__(url='https://amazon.com')


#class _AmazonSearchResultsPage(_GenericPage):
#  def __init__(self):
#    super(_AmazonSearchResultsPage,
#          self).__init__(url='https://amazon.com/s?k=performance')


class _BbcPage(_GenericPage):
  def __init__(self):
    super(_BbcPage, self).__init__(
        url='https://bbc.co.uk',
        accept_cookies_button_selector='button[id=bbccookies-continue-button]',
        scroll_repeat_count=10)


class _CnnPage(_GenericPage):
  def __init__(self):
    super(_CnnPage, self).__init__(
        url='https://www.cnn.com',
        scroll_repeat_count=10,
        accept_cookies_button_selector='button[id=onetrust-accept-btn-handler]')


class _EspnPage(_GenericPage):
  def __init__(self):
    super(_EspnPage, self).__init__(
        url='https://www.espn.com',
        accept_cookies_button_selector='button[id=onetrust-accept-btn-handler]')


class _EtsyPage(_GenericPage):
  def __init__(self):
    super(_EtsyPage,
          self).__init__(url='https://www.etsy.com',
                         accept_cookies_button_selector=
                         'button[data-gdpr-single-choice-accept=true]')


class _GoogleSearchResultsPage(_GenericPage):
  def __init__(self):
    super(_GoogleSearchResultsPage,
          self).__init__(url='https://google.com/search?q=performance')


class _IkeaPage(_GenericPage):
  def __init__(self):
    super(_IkeaPage, self).__init__(
        url='https://www.ikea.com/gb/en',
        accept_cookies_button_selector='button[id=onetrust-accept-btn-handler]')


class _ImdbPage(_GenericPage):
  def __init__(self):
    super(_ImdbPage, self).__init__(url='https://m.imdb.com')


class _NYTimesPage(_GenericPage):
  def __init__(self):
    super(_NYTimesPage, self).__init__(url='https://www.nytimes.com')

  # A popup shows for a while and automatically goes away. Wait for it to go
  # # away before clicking the cookie
  def AllowCookies(self, action_runner):
    action_runner.Wait(5)
    super(_NYTimesPage, self).AllowCookies(action_runner)


#class _RedditPage(_GenericPage):
#  def __init__(self):
#    super(_RedditPage, self).__init__(url='https://reddit.com')

#  def AllowCookies(self, action_runner):
#    action_runner.TapElement(selector='button[class=XPromoPopup__actionButton]')

#  Does not work because cookie accept is in an iframe and TapElement does not
# go into iframes (See BusinessInsiderMobile2021)
# class _TheGuardianPage(_GenericPage):
#   def __init__(self):
#     super(_TheGuardianPage, self).__init__(
#         url='https://www.theguardian.com')

#   def AllowCookies(self, action_runner):
#     self.MaybeTapElement(action_runner, text='Yes, I'm happy')


class _TwitterProfilePage(_GenericPage):
  def __init__(self):
    super(_TwitterProfilePage,
          self).__init__(url='https://mobile.twitter.com/nasa',
                         scroll_repeat_count=10)

  def AllowCookies(self, action_runner):
    self.MaybeTapElement(action_runner, text='Not now')
    self.MaybeTapElement(action_runner, text='Accept all cookies')


class _WikipediaArticlePage(_GenericPage):
  def __init__(self):
    super(_WikipediaArticlePage,
          self).__init__(url='https://en.m.wikipedia.org/wiki/Computer_program')


class _YahooNewsPage(_GenericPage):
  def __init__(self):
    super(_YahooNewsPage, self).__init__(url='https://news.yahoo.com/',
                                         scroll_repeat_count=10)

  def AllowCookies(self, action_runner):
    selector = 'button[name=agree]'
    if not self.IsElementPresent(action_runner, selector=selector):
      return
    action_runner.ScrollPage(distance=800, use_touch=True)
    action_runner.Wait(1)
    action_runner.TapElement(selector=selector)


class _YoutubePage(_GenericPage):
  def __init__(self):
    super(_YoutubePage, self).__init__(url='https://youtube.com')


# TODO: Add Amazon search results, Instagram, Facebook, Reddit, levi.com, theguardian


def _GetAllPages():
  classes = []
  for _, obj in inspect.getmembers(sys.modules[__name__]):
    if not inspect.isclass(obj):
      continue
    if not issubclass(obj, _GenericPage):
      continue
    # Exclude the _GenericPage itself.
    if obj is _GenericPage:
      continue
    classes.append(obj)
  return classes


class _PowerSharedState(SharedPageState):
  def ShouldReuseBrowserForAllStoryRuns(self):
    return True


class TopSitesStory(Page):
  def __init__(self, story_set, name='power:scroll:top'):
    super(TopSitesStory,
          self).__init__(page_set=story_set,
                         shared_page_state_class=_PowerSharedState,
                         name=name,
                         url="about:blank")

    self._pages = [c() for c in _GetAllPages()]

  def RunNavigateSteps(self, action_runner):
    for page in self._pages:
      page.RunNavigateSteps(action_runner)
      page.RunPageInteractions(action_runner)
      action_runner.Wait(10)

  def RunPageInteractions(self, action_runner):
    pass


class ContribPowerMobileTopSitesStorySet(StorySet):
  def __init__(self):
    super(ContribPowerMobileTopSitesStorySet,
          self).__init__(archive_data_file='data/contrib_power_mobile.json',
                         base_dir=os.path.dirname(os.path.abspath(__file__)))
    self.AddStory(TopSitesStory(self))
