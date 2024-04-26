# encoding: utf-8
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


# The number of lines will be reduced after 2018 update is complete and
# the old stories are removed: https://crbug.com/878390.
# pylint: disable=too-many-lines

import re

from page_sets.system_health import platforms
from page_sets.system_health import story_tags
from page_sets.system_health import system_health_story

from page_sets.login_helpers import autocad_login
from page_sets.login_helpers import facebook_login
from page_sets.login_helpers import google_login
from page_sets.login_helpers import pinterest_login
from page_sets.login_helpers import tumblr_login

from page_sets.helpers import override_online

from py_utils import TimeoutException

from telemetry.core import exceptions
from telemetry.util import js_template
from telemetry.util import wpr_modes


class _BrowsingStory(system_health_story.SystemHealthStory):
  """Abstract base class for browsing stories.

  A browsing story visits items on the main page. Subclasses provide
  CSS selector to identify the items and implement interaction using
  the helper methods of this class.
  """

  IS_SINGLE_PAGE_APP = False
  ITEM_SELECTOR = NotImplemented
  # Defaults to using the body element if not set.
  CONTAINER_SELECTOR = None
  ABSTRACT_STORY = True

  def __init__(self, story_set, take_memory_measurement,
               extra_browser_args=None):
    super(_BrowsingStory, self).__init__(story_set,
        take_memory_measurement, extra_browser_args)
    self.script_to_evaluate_on_commit = override_online.ALWAYS_ONLINE

  def _WaitForNavigation(self, action_runner):
    if not self.IS_SINGLE_PAGE_APP:
      action_runner.WaitForNavigate()

  def _NavigateToItem(self, action_runner, index):
    item_selector = js_template.Render(
        'document.querySelectorAll({{ selector }})[{{ index }}]',
        selector=self.ITEM_SELECTOR, index=index)
    # Only scrolls if element is not currently in viewport.
    action_runner.WaitForElement(element_function=item_selector)
    action_runner.ScrollPageToElement(
        element_function=item_selector,
        container_selector=self.CONTAINER_SELECTOR)
    self._ClickLink(action_runner, item_selector)

  def _ClickLink(self, action_runner, element_function):
    action_runner.WaitForElement(element_function=element_function)
    action_runner.ClickElement(element_function=element_function)
    self._WaitForNavigation(action_runner)

  def _NavigateBack(self, action_runner):
    action_runner.NavigateBack()
    self._WaitForNavigation(action_runner)

  @classmethod
  def GenerateStoryDescription(cls):
    return 'Load %s and navigate to some items/articles.' % cls.URL


class _ArticleBrowsingStory(_BrowsingStory):
  """Abstract base class for user stories browsing news / shopping articles.

  An article browsing story imitates browsing a articles:
  1. Load the main page.
  2. Open and scroll the first article.
  3. Go back to the main page and scroll it.
  4. Open and scroll the second article.
  5. Go back to the main page and scroll it.
  6. etc.
  """

  ITEM_READ_TIME_IN_SECONDS = 3
  ITEM_SCROLL_REPEAT = 2
  ITEMS_TO_VISIT = 4
  MAIN_PAGE_SCROLL_REPEAT = 0
  ABSTRACT_STORY = True
  # Some devices take long to load news webpages crbug.com/713036. Set to None
  # because we cannot access DEFAULT_WEB_CONTENTS_TIMEOUT from this file.
  COMPLETE_STATE_WAIT_TIMEOUT = None
  # On some pages (for ex: facebook) articles appear only after we start
  # scrolling. This specifies if we need scroll main page.
  SCROLL_BEFORE_BROWSE = False
  # In some cases we want to measure performance while we're loading. This
  # introduces a lot of variability and should be used cautiously.
  SCROLL_DURING_LOADING = False

  def _DidLoadDocument(self, action_runner):
    self._AfterNavigate(action_runner)
    # Scroll main page if needed before we start browsing articles.
    if self.SCROLL_BEFORE_BROWSE:
      self._ScrollMainPage(action_runner)
    for i in range(self.ITEMS_TO_VISIT):
      self._NavigateToItem(action_runner, i)
      self._AfterNavigate(action_runner)
      self._ReadNextArticle(action_runner)
      self._NavigateBack(action_runner)
      self._AfterNavigate(action_runner)
      self._ScrollMainPage(action_runner)

  def _AfterNavigate(self, action_runner):
    pass

  def _ReadNextArticle(self, action_runner):
    if not self.SCROLL_DURING_LOADING:
      if self.COMPLETE_STATE_WAIT_TIMEOUT is not None:
        action_runner.tab.WaitForDocumentReadyStateToBeComplete(
            timeout=self.COMPLETE_STATE_WAIT_TIMEOUT)
      else:
        action_runner.tab.WaitForDocumentReadyStateToBeComplete()
      action_runner.Wait(self.ITEM_READ_TIME_IN_SECONDS / 2.0)
    else:
      action_runner.tab.WaitForDocumentReadyStateToBeInteractiveOrBetter()
    action_runner.RepeatableBrowserDrivenScroll(
        repeat_count=self.ITEM_SCROLL_REPEAT)
    action_runner.Wait(self.ITEM_READ_TIME_IN_SECONDS/2.0)

  def _ScrollMainPage(self, action_runner):
    if not self.SCROLL_DURING_LOADING:
      action_runner.tab.WaitForDocumentReadyStateToBeComplete()
    else:
      action_runner.tab.WaitForDocumentReadyStateToBeInteractiveOrBetter()
    action_runner.RepeatableBrowserDrivenScroll(
        repeat_count=self.MAIN_PAGE_SCROLL_REPEAT)


##############################################################################
# News browsing stories.
##############################################################################


class CnnStory2021(_ArticleBrowsingStory):
  """The second top website in http://www.alexa.com/topsites/category/News"""
  NAME = 'browse:news:cnn:2021'
  URL = 'http://edition.cnn.com/'
  ITEM_SELECTOR = '.cd__content > h3 > a'
  ITEMS_TO_VISIT = 2
  TAGS = [
      story_tags.HEALTH_CHECK, story_tags.JAVASCRIPT_HEAVY, story_tags.YEAR_2021
  ]


class BusinessInsiderMobile2021(_ArticleBrowsingStory):
  """A newsite where we've seen janky performance in bug reports"""
  NAME = 'browse:news:businessinsider:2021'
  URL = 'https://www.businessinsider.com/'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  ITEM_SELECTOR = '.three-column > .tout-title-link'
  ITEMS_TO_VISIT = 3
  MAIN_PAGE_SCROLL_REPEAT = 2
  ITEM_SCROLL_REPEAT = 4
  TAGS = [story_tags.JAVASCRIPT_HEAVY, story_tags.YEAR_2021]
  SCROLL_BEFORE_BROWSE = True
  SCROLL_DURING_LOADING = False
  _ACCEPTED_COOKIE = {
      'businessinsider': '#sp_message_iframe_364841',
      'insider': '#sp_message_iframe_364844'
  }

  def _GetCookieContextId(self, tab):
    contexts = tab.EnableAllContexts().copy()
    for context in contexts:
      try:
        result = tab.EvaluateJavaScript(
            'document.querySelector(".message-button") != null;',
            context_id=context)
      except exceptions.EvaluateException:
        continue
      if result:
        return context
    return None

  def _AfterNavigate(self, action_runner):
    if self.SCROLL_DURING_LOADING:
      action_runner.tab.WaitForDocumentReadyStateToBeInteractiveOrBetter()
    else:
      action_runner.tab.WaitForDocumentReadyStateToBeComplete()
    # We want to clear any cookie.
    url = re.search(r'https://(www\.)?([^.]+\.)?([^.]+)\.com.*',
                    action_runner.tab.GetUrl())
    if url is None:
      raise RuntimeError("no matching for " + action_runner.tab.GetUrl() +
                         " using " + url.group(2))
    iframe = self._ACCEPTED_COOKIE[url.group(3)]
    if iframe != '':
      try:
        action_runner.WaitForElement(iframe, timeout_in_seconds=1)
      except TimeoutException:
        # Sometimes the cookie pop up doesn't appear.
        return
      cookie_context = self._GetCookieContextId(action_runner.tab)
      if cookie_context is not None:
        self._ACCEPTED_COOKIE[url.group(3)] = ''
        action_runner.ExecuteJavaScript(
            ('document.querySelectorAll(".message-button")[0].dispatchEvent('
             'new MouseEvent("click", {bubbles: true, cancellable: true}));'),
            context_id=cookie_context,
            user_gesture=True)


class BusinessInsiderScrollWhileLoadingMobile2021(BusinessInsiderMobile2021):
  """A newsite where we've seen janky performance in bug reports"""
  NAME = 'browse:news:businessinsider:loading:2021'
  SCROLL_DURING_LOADING = True
  # This is only used in system_health.scroll_jank at the moment. So to avoid
  # running it on all bots we say no platform and explicitly add it in
  # janky_story_set.py.
  SUPPORTED_PLATFORMS = platforms.NO_PLATFORMS


class FacebookMobileStory2019(_ArticleBrowsingStory):
  NAME = 'browse:social:facebook:2019'
  URL = 'https://www.facebook.com/rihanna'
  ITEM_SELECTOR = '._5msj'
  MAIN_PAGE_SCROLL_REPEAT = 1
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  IS_SINGLE_PAGE_APP = True
  SCROLL_BEFORE_BROWSE = True

  TAGS = [story_tags.EMERGING_MARKET, story_tags.YEAR_2019]

  def _Login(self, action_runner):
    facebook_login.LoginWithMobileSite(action_runner, 'facebook4')

  def _ScrollMainPage(self, action_runner):
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()
    # Facebook loads content dynamically. So keep trying to scroll till we find
    # the elements. Retry 5 times waiting a bit each time.
    for _ in range(5):
      action_runner.RepeatableBrowserDrivenScroll(
          repeat_count=self.MAIN_PAGE_SCROLL_REPEAT)
      result = action_runner.EvaluateJavaScript(
          'document.querySelectorAll("._5msj").length')
      if result:
        break
      action_runner.Wait(1)


class FacebookDesktopStory(_ArticleBrowsingStory):
  NAME = 'browse:social:facebook'
  URL = 'https://www.facebook.com/rihanna'
  ITEM_SELECTOR = '._4-eo'
  IS_SINGLE_PAGE_APP = True
  # Web-page-replay does not work for this website:
  # https://github.com/chromium/web-page-replay/issues/79.
  SUPPORTED_PLATFORMS = platforms.NO_PLATFORMS
  TAGS = [story_tags.YEAR_2016]


class InstagramMobileStory2019(_ArticleBrowsingStory):
  NAME = 'browse:social:instagram:2019'
  URL = 'https://www.instagram.com/badgalriri/'
  ITEM_SELECTOR = '[class="v1Nh3 kIKUG  _bz0w"] a'
  ITEMS_TO_VISIT = 8

  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.EMERGING_MARKET, story_tags.YEAR_2019]

  def _WaitForNavigation(self, action_runner):
    action_runner.WaitForElement(selector='[title="badgalriri"]')

  def _NavigateBack(self, action_runner):
    action_runner.NavigateBack()


class FlipboardDesktopStory2020(_ArticleBrowsingStory):
  NAME = 'browse:news:flipboard:2020'
  URL = 'https://flipboard.com/explore'
  IS_SINGLE_PAGE_APP = True
  ITEM_SELECTOR = '.cover-image'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2020]


class HackerNewsDesktopStory2020(_ArticleBrowsingStory):
  NAME = 'browse:news:hackernews:2020'
  URL = 'https://news.ycombinator.com'
  ITEM_SELECTOR = '.athing .title > a'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2020]


class NytimesDesktopStory2020(_ArticleBrowsingStory):
  """
  The third top website in http://www.alexa.com/topsites/category/News
  Known Replay Errors:
  - window.EventTracker is not loaded
  - all network errors are related to ads
  """
  NAME = 'browse:news:nytimes:2020'
  URL = 'http://www.nytimes.com'
  ITEM_SELECTOR = "a[href*='/2020/']"
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2020]


class NytimesMobileStory2019(_ArticleBrowsingStory):
  """The third top website in http://www.alexa.com/topsites/category/News"""
  NAME = 'browse:news:nytimes:2019'
  URL = 'http://mobile.nytimes.com'
  ITEM_SELECTOR = '.css-1yjtett a'
  # Nytimes is very heavy so only visit 2 articles.
  ITEMS_TO_VISIT = 2
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.YEAR_2019]


class QqMobileStory2019(_ArticleBrowsingStory):
  NAME = 'browse:news:qq:2019'
  URL = 'https://xw.qq.com/#news'
  ITEM_SELECTOR = '.title'
  # The page seems to get stuck after three navigations.
  ITEMS_TO_VISIT = 2

  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.INTERNATIONAL, story_tags.YEAR_2019]


class RedditDesktopStory2020(_ArticleBrowsingStory):
  """The top website in http://www.alexa.com/topsites/category/News"""
  NAME = 'browse:news:reddit:2020'
  URL = 'https://www.reddit.com/r/news/top/?sort=top&t=week'
  IS_SINGLE_PAGE_APP = True
  ITEM_SELECTOR = 'article'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2020]


class RedditMobileStory2019(_ArticleBrowsingStory):
  NAME = 'browse:news:reddit:2019'
  URL = 'https://www.reddit.com/r/news/top/?sort=top&t=week'
  IS_SINGLE_PAGE_APP = True
  ITEM_SELECTOR = '.PostHeader__post-title-line'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.HEALTH_CHECK, story_tags.YEAR_2019]

  def _DidLoadDocument(self, action_runner):
    # We encountered ads disguised as articles on the Reddit one so far. The
    # following code skips that ad.
    # If we encounter it more often it will make sense to have a more generic
    # approach, e.g an OFFSET to start iterating from, or an index to skip.

    # Add one to the items to visit since we are going to skip the ad and we
    # want to still visit the same amount of articles.
    for i in range(self.ITEMS_TO_VISIT + 1):
      # Skip the ad disguised as an article.
      if i == 1:
        continue
      self._NavigateToItem(action_runner, i)
      self._ReadNextArticle(action_runner)
      self._NavigateBack(action_runner)
      self._ScrollMainPage(action_runner)


class TwitterMobileStory2019(_ArticleBrowsingStory):
  NAME = 'browse:social:twitter:2019'
  URL = 'https://www.twitter.com/nasa'
  ITEM_SELECTOR = ('[class="css-901oao r-hkyrab r-1qd0xha r-1b43r93 r-16dba41 '
      'r-ad9z0x r-bcqeeo r-bnwqim r-qvutc0"]')
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.HEALTH_CHECK, story_tags.YEAR_2019]

  def _WaitForNavigation(self, action_runner):
    action_runner.WaitForElement(selector=('[class="css-901oao css-16my406 '
        'r-1qd0xha r-ad9z0x r-bcqeeo r-qvutc0"]'))

class TwitterDesktopStory2018(_ArticleBrowsingStory):
  NAME = 'browse:social:twitter:2018'
  URL = 'https://www.twitter.com/nasa'
  IS_SINGLE_PAGE_APP = True
  ITEM_SELECTOR = '.tweet-text'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2018]


class WashingtonPostMobileStory2019(_ArticleBrowsingStory):
  """Progressive website"""
  NAME = 'browse:news:washingtonpost:2019'
  URL = 'https://www.washingtonpost.com/pwa'
  IS_SINGLE_PAGE_APP = True
  ITEM_SELECTOR = '.headline > a'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  _BROWSE_FREE_SELECTOR = '[class="continue-btn button free"]'
  _I_AGREE_SELECTOR = '.agree-ckb'
  _CONTINUE_SELECTOR = '[class="continue-btn button accept-consent"]'
  TAGS = [story_tags.YEAR_2019]

  def _DidLoadDocument(self, action_runner):
    # Get past GDPR and subscription dialog.
    action_runner.WaitForElement(selector=self._BROWSE_FREE_SELECTOR)
    action_runner.ClickElement(selector=self._BROWSE_FREE_SELECTOR)
    action_runner.WaitForElement(selector=self._I_AGREE_SELECTOR)
    action_runner.ClickElement(selector=self._I_AGREE_SELECTOR)
    action_runner.ClickElement(selector=self._CONTINUE_SELECTOR)

    super(WashingtonPostMobileStory2019, self)._DidLoadDocument(action_runner)


##############################################################################
# Search browsing stories.
##############################################################################


class GoogleAmpStory2018(_ArticleBrowsingStory):
  """ Story for Google's Accelerated Mobile Pages (AMP).

    The main thing we care about measuring here is load, so just query for
    news articles and then load the first amp link.
  """
  NAME = 'browse:search:amp:2018'
  URL = 'https://www.google.com/search?q=news&hl=en'
  # Need to find the first card in the news section that has an amp
  # indicator on it
  ITEM_SELECTOR = '.sm62ie > a[class*="amp_r"]'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.YEAR_2018]

  def _DidLoadDocument(self, action_runner):
    # Click on the amp news link and then just wait for it to load.
    element_function = js_template.Render(
        'document.querySelectorAll({{ selector }})[{{ index }}]',
        selector=self.ITEM_SELECTOR, index=0)
    action_runner.WaitForElement(element_function=element_function)
    action_runner.ClickElement(element_function=element_function)
    action_runner.Wait(2)

class GoogleAmpSXGStory2019(_ArticleBrowsingStory):
  """ Story for Google's Signed Exchange (SXG) Accelerated Mobile Pages (AMP).
  """
  NAME = 'browse:search:amp:sxg:2019'
  # Specific URL for site that supports SXG, travel.yahoo.co.jp
  # pylint: disable=line-too-long
  URL='https://www.google.com/search?q=%E5%85%AD%E6%9C%AC%E6%9C%A8%E3%80%80%E3%83%A4%E3%83%95%E3%83%BC%E3%80%80%E3%83%9B%E3%83%86%E3%83%AB&esrch=SignedExchange::Demo'
  # Need to find the SXG AMPlink in the results
  ITEM_SELECTOR = 'a > div > span[aria-label="AMP logo"]'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.YEAR_2019]

  def _DidLoadDocument(self, action_runner):
    # Waiting manually for the search results to load here and below.
    # Telemetry's action_runner.WaitForNavigate has some difficulty with amp
    # pages as it waits for a frameId without a parent id.
    action_runner.Wait(2)
    # Click on the yahoo amp link and then just wait for it to load.
    element_function = js_template.Render(
        'document.querySelectorAll({{ selector }})[{{ index }}]',
        selector=self.ITEM_SELECTOR, index=0)
    action_runner.WaitForElement(element_function=element_function)
    action_runner.ClickElement(element_function=element_function)
    # Waiting for the document to fully render
    action_runner.Wait(2)


class GoogleDesktopStory2018(_ArticleBrowsingStory):
  """
  A typical google search story:
    _ Start at https://www.google.com/search?q=flower
    _ Click on the wikipedia link & navigate to
      https://en.wikipedia.org/wiki/Flower
    _ Scroll down the wikipedia page about flower.
    _ Back to the search main page.
    _ Refine the search query to 'delivery flower'.
    _ Scroll down the page.
    _ Click the next page result of 'delivery flower'.
    _ Scroll the search page.

  """
  NAME = 'browse:search:google:2020'
  URL = 'https://www.google.com/search?q=flower&hl=en'
  _SEARCH_BOX_SELECTOR = 'input[aria-label="Search"]'
  _SEARCH_PAGE_2_SELECTOR = 'a[aria-label="Page 2"]'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2020]

  def _DidLoadDocument(self, action_runner):
    # Click on flower Wikipedia link.
    action_runner.Wait(2)
    action_runner.ClickElement(text='Flower - Wikipedia')
    action_runner.WaitForNavigate()

    # Scroll the flower Wikipedia page, then navigate back.
    action_runner.Wait(2)
    action_runner.ScrollPage()
    action_runner.Wait(2)
    action_runner.NavigateBack()

    # Click on the search box.
    action_runner.WaitForElement(selector=self._SEARCH_BOX_SELECTOR)
    action_runner.ExecuteJavaScript(
        'document.querySelector({{ selector }}).focus()',
        selector=self._SEARCH_BOX_SELECTOR)
    action_runner.Wait(2)

    # Submit search query.
    action_runner.EnterText('delivery ')
    action_runner.Wait(0.5)
    action_runner.PressKey('Return')

    # Scroll down & click next search result page.
    action_runner.Wait(2)
    action_runner.ScrollPageToElement(selector=self._SEARCH_PAGE_2_SELECTOR)
    action_runner.Wait(2)
    action_runner.ClickElement(selector=self._SEARCH_PAGE_2_SELECTOR)
    action_runner.Wait(2)
    action_runner.ScrollPage()


class GoogleIndiaDesktopStory2021(_ArticleBrowsingStory):
  """
  A typical google search story in India:
    1. Start at self.URL
    2. Scroll down the page.
    3. Refine the query & click search box
    4. Scroll down the page.
    5. Click the next page result
    6. Scroll the search result page.

  """
  NAME = 'browse:search:google_india:2021'
  URL = 'https://www.google.co.in/search?q=%E0%A4%AB%E0%A5%82%E0%A4%B2&hl=hi'
  _SEARCH_BOX_SELECTOR = 'input[name="q"]'
  _SEARCH_BUTTON_SELECTOR = 'button[aria-label="Google Search"]'
  _SEARCH_PAGE_2_SELECTOR = 'a[aria-label="Page 2"]'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.INTERNATIONAL, story_tags.YEAR_2021]

  def _DidLoadDocument(self, action_runner):
    # Refine search query in the search box.
    action_runner.WaitForElement(self._SEARCH_BOX_SELECTOR)
    action_runner.ExecuteJavaScript(
        'document.querySelector({{ selector }}).select()',
        selector=self._SEARCH_BOX_SELECTOR)
    action_runner.Wait(1)
    action_runner.EnterText(u'वितरण', character_delay_ms=250)
    action_runner.Wait(2)
    action_runner.ClickElement(selector=self._SEARCH_BUTTON_SELECTOR)

    # Scroll down & click next search result page.
    action_runner.Wait(2)
    action_runner.ScrollPageToElement(selector=self._SEARCH_PAGE_2_SELECTOR)
    action_runner.Wait(2)
    action_runner.ClickElement(selector=self._SEARCH_PAGE_2_SELECTOR)
    action_runner.Wait(2)
    action_runner.ScrollPage()


##############################################################################
# Media browsing stories.
##############################################################################


class _MediaBrowsingStory(_BrowsingStory):
  """Abstract base class for media user stories

  A media story imitates browsing a website with photo or video content:
  1. Load a page showing a media item
  2. Click on the next link to go to the next media item
  3. etc.
  """

  ABSTRACT_STORY = True
  ITEM_VIEW_TIME_IN_SECONDS = 3
  ITEMS_TO_VISIT = 15
  ITEM_SELECTOR_INDEX = 0
  INCREMENT_INDEX_AFTER_EACH_ITEM = False

  def _DidLoadDocument(self, action_runner):
    index = self.ITEM_SELECTOR_INDEX
    for _ in range(self.ITEMS_TO_VISIT):
      self._NavigateToItem(action_runner, index)
      self._ViewMediaItem(action_runner, index)
      if self.INCREMENT_INDEX_AFTER_EACH_ITEM:
        index += 1


  def _ViewMediaItem(self, action_runner, index):
    del index  # Unused.
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()
    action_runner.Wait(self.ITEM_VIEW_TIME_IN_SECONDS)


class ImgurMobileStory2019(_MediaBrowsingStory):
  NAME = 'browse:media:imgur:2019'
  URL = 'http://imgur.com/gallery/46DfUFT'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  IS_SINGLE_PAGE_APP = True
  TAGS = [story_tags.EMERGING_MARKET, story_tags.YEAR_2019]
  USER_READ_TIME = 1

  def _DidLoadDocument(self, action_runner):
    # Accept the cookies
    accept_button = ".qc-cmp-button"
    item_selector = js_template.Render(
       'document.querySelectorAll({{ selector }})[{{ index }}]',
       selector=accept_button, index=1)
    action_runner.WaitForElement(element_function=item_selector)
    action_runner.ClickElement(element_function=item_selector)
    # To simulate user looking at image
    action_runner.Wait(self.USER_READ_TIME)

    # Keep scrolling for the specified amount. If we see "continue browse"
    # button click it to enable further scroll. This button would only be added
    # after we scrolled a bit. So can't wait for this button at the start.
    accepted_continue = False
    for _ in range(15):
      result = action_runner.EvaluateJavaScript(
          'document.querySelectorAll(".Button-tertiary").length')
      if result and not accepted_continue:
        accept_button = ".Button-tertiary"
        item_selector = js_template.Render(
           'document.querySelectorAll({{ selector }})[{{ index }}]',
           selector=accept_button, index=0)
        action_runner.ScrollPageToElement(element_function=item_selector,
                                          speed_in_pixels_per_second=400,
                                          container_selector=None)
        action_runner.ClickElement(element_function=item_selector)
        accepted_continue = True

      action_runner.ScrollPage(distance=800)
      # To simulate user looking at image
      action_runner.Wait(self.USER_READ_TIME)


class ImgurDesktopStory(_MediaBrowsingStory):
  NAME = 'browse:media:imgur'
  URL = 'http://imgur.com/gallery/5UlBN'
  ITEM_SELECTOR = '.navNext'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  IS_SINGLE_PAGE_APP = True
  TAGS = [story_tags.YEAR_2016]


class TikTokMobileStory2021(_BrowsingStory):
  NAME = 'browse:media:tiktok_infinite_scroll:2021'
  URL = 'https://tiktok.com/'
  TAGS = [story_tags.INFINITE_SCROLL, story_tags.YEAR_2021]
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY

  _TIME_TO_WAIT_BEFORE_STARTING_IN_SECONDS = 2
  _TIME_TO_WAIT_BETWEEN_VIDEOS = 1
  _ACCEPT_ALL_SELECTOR = 'div[class$=" cookie-banner"]>div[class$=" button-wrapper"]>button'

  def _DidLoadDocument(self, action_runner):
    # Accept all cookies
    action_runner.WaitForElement(selector=self._ACCEPT_ALL_SELECTOR)
    action_runner.ClickElement(selector=self._ACCEPT_ALL_SELECTOR)
    action_runner.Wait(self._TIME_TO_WAIT_BEFORE_STARTING_IN_SECONDS)

    # TikTok doesn't scroll like a traditional page but responds to vertical
    # swipe gestures.
    for direction in ['down', 'up', 'down']:
      for _ in range(0, 3):
        scroll_dist = action_runner.EvaluateJavaScript(
            'window.innerHeight') * 0.8
        action_runner.ScrollPage(distance=scroll_dist, direction=direction)
        action_runner.Wait(self._TIME_TO_WAIT_BETWEEN_VIDEOS)


class YouTubeMobileStory2019(_MediaBrowsingStory):
  """Load a typical YouTube video then navigate to a next few videos. Stop and
  watch each video for few seconds.
  """
  NAME = 'browse:media:youtube:2019'
  URL = 'https://m.youtube.com/watch?v=TcMBFSGVi1c&autoplay=false'
  ITEM_SELECTOR = '.compact-media-item > a'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  IS_SINGLE_PAGE_APP = True
  ITEM_SELECTOR_INDEX = 3
  ITEMS_TO_VISIT = 8
  TAGS = [
      story_tags.JAVASCRIPT_HEAVY, story_tags.EMERGING_MARKET,
      story_tags.YEAR_2019
  ]


class YouTubeDesktopStory2019(_MediaBrowsingStory):
  """Load a typical YouTube video then navigate to a next few videos. Stop and
  watch each video for a few seconds.
  """
  NAME = 'browse:media:youtube:2019'
  URL = 'https://www.youtube.com/watch?v=QGfhS1hfTWw&autoplay=0'
  ITEM_SELECTOR = 'ytd-compact-video-renderer.ytd-watch-next-secondary-results-renderer a'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  IS_SINGLE_PAGE_APP = True
  # A longer view time allows videos to load and play.
  ITEM_VIEW_TIME_IN_SECONDS = 5
  ITEMS_TO_VISIT = 8
  ITEM_SELECTOR_INDEX = 3
  PLATFORM_SPECIFIC = True
  TAGS = [story_tags.JAVASCRIPT_HEAVY, story_tags.YEAR_2019]


class PhotoshopDesktopStory2021(_MediaBrowsingStory):
  """Photoshop desktop story,
  Measure the time it takes to open a shared Photoshop file.
  """
  NAME = 'browse:tools:photoshop:2021'
  URL = 'https://photoshop.adobe.com/id/urn:aaid:sc:EU:1856a1e7-f397-4616-b399-9cd3b3d8c029'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2021, story_tags.WEBASSEMBLY]

  # This map translates page-specific event names to event names needed for
  # the reported_by_page:* metric.
  EVENTS_REPORTED_BY_PAGE = '''
    window.__telemetry_reported_page_events = {
      'Doc.open':
          'telemetry:reported_by_page:benchmark_begin',
      'Doc.open complete':
          'telemetry:reported_by_page:benchmark_end',
      'open document end':
          'telemetry:reported_by_page:interactive'
    };
  '''

  # Patch performance.mark to get notified about page events.
  PERFORMANCE_MARK_PATCH = '''
    window.__telemetry_observed_page_events = new Set();
    (function () {
      let reported = window.__telemetry_reported_page_events;
      let observed = window.__telemetry_observed_page_events;
      let performance_mark = window.performance.mark;
      window.performance.mark = function (label) {
        performance_mark.call(window.performance, label);
        if (reported.hasOwnProperty(label)) {
          performance_mark.call(
              window.performance, reported[label]);
          observed.add(reported[label]);
        }
      }
    })();
  '''

  # Page event queries.
  FINISHED_EVENT = '''
    (window.__telemetry_observed_page_events.has(
        "telemetry:reported_by_page:benchmark_end"))
  '''

  def __init__(self, story_set, take_memory_measurement):
    super(PhotoshopDesktopStory2021, self).__init__(story_set,
                                                    take_memory_measurement)
    self.script_to_evaluate_on_commit = js_template.Render(
        '''{{@events_reported_by_page}}
        {{@performance_mark}}''',
        events_reported_by_page=self.EVENTS_REPORTED_BY_PAGE,
        performance_mark=self.PERFORMANCE_MARK_PATCH)

  def _DidLoadDocument(self, action_runner):
    action_runner.WaitForJavaScriptCondition(self.FINISHED_EVENT)


class PhotoshopWarmStartupDesktopStory2021(_MediaBrowsingStory):
  """Photoshop desktop story, measuring warm startup,
  Open Photoshop, wait for it to load, then navigate to a different url to
  get rid of the original renderer process, and then navigate back to Photoshop
  to do the actual measurement. The story then measures the time it takes to
  open a shared Photoshop file.
  """
  NAME = 'browse:tools:photoshop_warm:2021'
  URL = 'https://photoshop.adobe.com/id/urn:aaid:sc:EU:1856a1e7-f397-4616-b399-9cd3b3d8c029'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2021, story_tags.WEBASSEMBLY]

  # This map translates page-specific event names to event names needed for
  # the reported_by_page:* metric.
  EVENTS_REPORTED_BY_PAGE = '''
    window.__telemetry_reported_page_events = {
      'Apollo.init()':
          'telemetry:reported_by_page:benchmark_begin',
      'Doc.open complete':
          'telemetry:reported_by_page:benchmark_end'
    };
  '''

  # Patch performance.mark to get notified about page events.
  PERFORMANCE_MARK_PATCH = '''
    window.__telemetry_observed_page_events = new Set();
    (function () {
      let reported = window.__telemetry_reported_page_events;
      let observed = window.__telemetry_observed_page_events;
      let performance_mark = window.performance.mark;
      window.performance.mark = function (label) {
        performance_mark.call(window.performance, label);
        if (reported.hasOwnProperty(label)) {
          performance_mark.call(
              window.performance, reported[label]);
          observed.add(reported[label]);
        }
      }
    })();
  '''

  WARMUP_RUNS_SCRIPT = '''
      let performance_mark = window.performance.mark;
      window.__telemetry_first_load_finished = false;
      window.performance.mark = function (label) {
        performance_mark.call(window.performance, label);
        if (label == 'Doc.open complete') {
          window.__telemetry_first_load_finished = true;
        }
      }
  '''

  # Page event queries.
  FINISHED_EVENT = '''
    (window.__telemetry_observed_page_events.has(
        "telemetry:reported_by_page:benchmark_end"))
  '''

  def __init__(self, story_set, take_memory_measurement):
    super(PhotoshopWarmStartupDesktopStory2021,
          self).__init__(story_set, take_memory_measurement)
    self.script_to_evaluate_on_commit = js_template.Render(
        self.WARMUP_RUNS_SCRIPT)

  def _DidLoadDocument(self, action_runner):
    action_runner.Wait(2)
    # The first navigation to photoshop initializes caching.
    action_runner.WaitForJavaScriptCondition(
        'window.__telemetry_first_load_finished')
    # Navigate to a different page to start a new renderer process. The
    # original process stores the wasm module of photoshop in an in-process
    # cache.
    action_runner.Navigate('https://www.google.com/')
    action_runner.Wait(1)
    warmup_script = js_template.Render(self.WARMUP_RUNS_SCRIPT)
    # The second navigation to photoshop stores the compiled wasm module in the
    # cache.
    action_runner.Navigate(
        'https://photoshop.adobe.com/id/urn:aaid:sc:EU:1856a1e7-f397-4616-b399-9cd3b3d8c029',
        warmup_script)
    action_runner.Wait(2)
    action_runner.WaitForJavaScriptCondition(
        'window.__telemetry_first_load_finished')
    # Wait for the optimizing compiler to finish compilation and to write the
    # compiled module into the cache.
    action_runner.Wait(30)
    # Reset the renderer process again.
    action_runner.Navigate('https://www.google.com/')
    action_runner.Wait(1)
    measurement_script = js_template.Render(
        '''{{@events_reported_by_page}}
        {{@performance_mark}}''',
        events_reported_by_page=self.EVENTS_REPORTED_BY_PAGE,
        performance_mark=self.PERFORMANCE_MARK_PATCH)
    # The third navigation loads photoshop from the cache. This is the page
    # load we want to measure.
    action_runner.Navigate(
        'https://photoshop.adobe.com/id/urn:aaid:sc:EU:1856a1e7-f397-4616-b399-9cd3b3d8c029',
        measurement_script)
    action_runner.Wait(2)
    action_runner.WaitForJavaScriptCondition(self.FINISHED_EVENT)


class AutoCADDesktopStory2021(_MediaBrowsingStory):
  """AutoCAD desktop story,
  TODO: add a description here.
  """
  NAME = 'browse:tools:autocad:2021'
  URL = 'https://web.autocad.com/?user=wasm-benchmark'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [
      story_tags.YEAR_2021, story_tags.WEBASSEMBLY, story_tags.WEBGL,
      story_tags.KEYBOARD_INPUT
  ]

  def __init__(self, story_set, take_memory_measurement):
    super(AutoCADDesktopStory2021, self).__init__(story_set,
                                                  take_memory_measurement)

  def _Login(self, action_runner):
    autocad_login.LoginWithDesktopSite(action_runner, 'autocad')

  def _DidLoadDocument(self, action_runner):
    action_runner.WaitForElement(text="Sign in")
    action_runner.ClickElement(text="Sign in")
    # Now we are done with the login.
    action_runner.WaitForElement(text="Samples")
    action_runner.ClickElement(text="Samples")

    action_runner.WaitForElement(text="Dog House Plan Sample.dwg")
    action_runner.ClickElement(text="Dog House Plan Sample.dwg")

    # We cannot wait for an element, because the result of the loading
    # action is just a drawing on the canvas.
    action_runner.Wait(10)
    action_runner.EnterText('MEASURE')
    action_runner.PressKey('Return')
    action_runner.Wait(1)
    action_runner.EnterText('-3,1')
    action_runner.PressKey('Return')

    action_runner.Wait(5)


class YouTubeTVDesktopStory2019(_MediaBrowsingStory):
  """Load a typical YouTube TV video then navigate to a next few videos. Stop
  and watch each video for a few seconds.
  """
  NAME = 'browse:media:youtubetv:2019'
  URL = 'https://www.youtube.com/tv#/watch/ads/control?v=PxrnoGyBw4E&resume'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2019]

  def WaitIfRecording(self, action_runner):
    # Uncomment the below if recording to try and reduce network errors.
    # action_runner.Wait(2)
    pass

  def WatchThenSkipAd(self, action_runner):
    skip_button_selector = '.skip-ad-button'
    action_runner.WaitForElement(selector=skip_button_selector)
    action_runner.Wait(8)  # Wait until the ad is skippable.
    action_runner.MouseClick(selector=skip_button_selector)
    self.WaitIfRecording(action_runner)

  def ShortAttentionSpan(self, action_runner):
    action_runner.Wait(2)

  def GotoNextVideo(self, action_runner):
    forward_button_selector = '.skip-forward-button'
    action_runner.PressKey('ArrowDown')   # Open the menu.
    action_runner.WaitForElement(selector=forward_button_selector)
    action_runner.MouseClick(selector=forward_button_selector)
    self.WaitIfRecording(action_runner)

  def NavigateInMenu(self, action_runner):
    short_delay_in_ms = 300
    delay_in_ms = 1000
    long_delay_in_ms = 3000
    # Escape to menu, skip the sign-in process.
    action_runner.PressKey('Backspace', 1, long_delay_in_ms)
    action_runner.PressKey('ArrowDown', 1, delay_in_ms)
    action_runner.PressKey('Return', 1, long_delay_in_ms)
    self.WaitIfRecording(action_runner)
    # Scroll through categories and back.
    action_runner.WaitForElement(selector='#guide-logo')
    action_runner.PressKey('ArrowUp', 1, delay_in_ms)
    action_runner.PressKey('ArrowRight', 3, delay_in_ms)
    action_runner.PressKey('ArrowLeft', 3, delay_in_ms)
    action_runner.PressKey('ArrowDown', 1, delay_in_ms)
    self.WaitIfRecording(action_runner)
    # Scroll through a few videos then open the sidebar menu.
    action_runner.PressKey('ArrowRight', 3, short_delay_in_ms)
    action_runner.PressKey('ArrowDown', 3, short_delay_in_ms)
    action_runner.PressKey('Backspace', 2, delay_in_ms)
    self.WaitIfRecording(action_runner)
    # Scroll through options and then go to search.
    action_runner.PressKey('ArrowDown', 3, delay_in_ms)
    action_runner.PressKey('s', 1, delay_in_ms)
    self.WaitIfRecording(action_runner)
    # Search for 'dub stories' and start playing.
    action_runner.EnterText('dub stories', short_delay_in_ms)
    action_runner.PressKey('ArrowDown', 1, delay_in_ms)
    action_runner.PressKey('Return', 2, delay_in_ms)
    self.WaitIfRecording(action_runner)

  def _DidLoadDocument(self, action_runner):
    self.WatchThenSkipAd(action_runner)
    self.ShortAttentionSpan(action_runner)
    self.GotoNextVideo(action_runner)
    self.ShortAttentionSpan(action_runner)
    self.GotoNextVideo(action_runner)
    self.ShortAttentionSpan(action_runner)
    self.NavigateInMenu(action_runner)

  # This story is mainly relevant for V8 in jitless mode, but there is no
  # benchmark that enables this flag. We take the pragmatic solution and set
  # this flag explicitly for this story.
  def __init__(self, story_set, take_memory_measurement):
    super(YouTubeTVDesktopStory2019, self).__init__(
        story_set, take_memory_measurement,
        extra_browser_args=['--js-flags="--jitless"'])


class YouTubeTVDesktopWatchStory2020(_MediaBrowsingStory):
  """Load a typical YouTube TV video then navigate to a next few videos. Stop
  and watch each video for a few seconds.
  """
  NAME = 'browse:media:youtubetv_watch:2020'
  URL = ('https://www.youtube.com/tv?'
         'env_adsUrl=http%3A%2F%2Fvastsynthesizer.appspot.com'
         '%2Fyshi_trv_instream_10s#/watch?v=Ylo257Av-qQ')
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2020]

  def WaitIfRecording(self, action_runner):
    # Uncomment the below if recording to try and reduce network errors.
    # action_runner.Wait(2)
    pass

  def WatchThenSkipAd(self, action_runner):
    skip_button_selector = '.skip-ad-button'
    action_runner.WaitForElement(selector=skip_button_selector)
    action_runner.Wait(8)  # Wait until the ad is skippable.
    action_runner.MouseClick(selector=skip_button_selector)
    self.WaitIfRecording(action_runner)

  def ShortAttentionSpan(self, action_runner):
    action_runner.Wait(2)

  def GotoNextVideo(self, action_runner):
    delay_in_ms = 1000
    action_runner.PressKey('ArrowDown', 1, delay_in_ms)
    action_runner.PressKey('ArrowRight', 1, delay_in_ms)
    action_runner.PressKey('Return', 2, delay_in_ms)
    self.WaitIfRecording(action_runner)

  def _DidLoadDocument(self, action_runner):
    self.WatchThenSkipAd(action_runner)
    self.ShortAttentionSpan(action_runner)
    self.GotoNextVideo(action_runner)
    self.WatchThenSkipAd(action_runner)
    self.ShortAttentionSpan(action_runner)
    self.GotoNextVideo(action_runner)
    self.WatchThenSkipAd(action_runner)
    self.ShortAttentionSpan(action_runner)
    self.GotoNextVideo(action_runner)
    self.WatchThenSkipAd(action_runner)
    self.ShortAttentionSpan(action_runner)

  # This story is mainly relevant for V8 in jitless mode, but there is no
  # benchmark that enables this flag. We take the pragmatic solution and set
  # this flag explicitly for this story.
  def __init__(self, story_set, take_memory_measurement):
    super(YouTubeTVDesktopWatchStory2020,
          self).__init__(story_set,
                         take_memory_measurement,
                         extra_browser_args=['--js-flags="--jitless"'])


class FacebookPhotosMobileStory2019(_MediaBrowsingStory):
  """Load a photo page from Rihanna's facebook page then navigate a few next
  photos.
  """
  NAME = 'browse:media:facebook_photos:2019'
  URL = (
      'https://m.facebook.com/rihanna/photos/a.10152251658271676/10156761246686676/?type=3&source=54')
  ITEM_SELECTOR = '._57-r.touchable'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  IS_SINGLE_PAGE_APP = True
  ITEM_SELECTOR_INDEX = 0
  TAGS = [story_tags.EMERGING_MARKET, story_tags.YEAR_2019]


class TumblrDesktopStory2018(_MediaBrowsingStory):
  NAME = 'browse:media:tumblr:2018'
  URL = 'https://tumblr.com/search/gifs'
  ITEM_SELECTOR = '.post_media'
  IS_SINGLE_PAGE_APP = True
  ITEMS_TO_VISIT = 8
  INCREMENT_INDEX_AFTER_EACH_ITEM = True
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2018]

  def _Login(self, action_runner):
    tumblr_login.LoginDesktopAccount(action_runner, 'tumblr')
    action_runner.Wait(3)

  def _ViewMediaItem(self, action_runner, index):
    super(TumblrDesktopStory2018, self)._ViewMediaItem(action_runner, index)
    action_runner.WaitForElement(selector='#tumblr_lightbox')
    action_runner.MouseClick(selector='#tumblr_lightbox')
    action_runner.Wait(1)  # To make browsing more realistic.



class PinterestDesktopStory2018(_MediaBrowsingStory):
  NAME = 'browse:media:pinterest:2018'
  URL = 'https://pinterest.com'
  ITEM_SELECTOR = '.pinWrapper a[data-force-refresh="1"]'
  ITEM_VIEW_TIME = 5
  IS_SINGLE_PAGE_APP = True
  ITEMS_TO_VISIT = 8
  INCREMENT_INDEX_AFTER_EACH_ITEM = True
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2018]
  # SKIP_LOGIN = False

  def _Login(self, action_runner):
    pinterest_login.LoginDesktopAccount(action_runner, 'googletest')

  def _ViewMediaItem(self, action_runner, index):
    super(PinterestDesktopStory2018, self)._ViewMediaItem(action_runner, index)
    # 1. click on item
    # 2. pin every other item
    # 3. go back to the main page
    action_runner.Wait(1)  # Wait to make navigation realistic.
    if index % 2 == 0:
      if not self.SKIP_LOGIN:
        action_runner.Wait(2)
      action_runner.WaitForElement(selector='.SaveButton')
      action_runner.ClickElement(selector='.SaveButton')
      if not self.SKIP_LOGIN:
        action_runner.Wait(2)
      action_runner.Wait(2.5)
      action_runner.WaitForElement(
                  selector='div[data-test-id=BoardPickerSaveButton]')
      action_runner.ClickElement(
                  selector='div[data-test-id=BoardPickerSaveButton]')
      action_runner.Wait(1.5)
    action_runner.Wait(1)
    if not self.SKIP_LOGIN:
      action_runner.Wait(2)
    action_runner.NavigateBack()

    action_runner.WaitForElement(selector='input[name=searchBoxInput]')
    action_runner.Wait(1)
    if not self.SKIP_LOGIN:
      action_runner.Wait(2)

class GooglePlayStoreMobileStory(_MediaBrowsingStory):
  """ Navigate to the movies page of Google Play Store, scroll to the bottom,
  and click "see more" of a middle category (last before second scroll).
  """
  NAME = 'browse:media:googleplaystore:2019'
  URL = 'https://play.google.com/store/movies'
  ITEM_SELECTOR = ''
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  IS_SINGLE_PAGE_APP = True
  TAGS = [story_tags.EMERGING_MARKET, story_tags.YEAR_2019, story_tags.IMAGES]
  # intends to select the last category of movies and its "see more" button
  _SEE_MORE_SELECTOR = ('div[class*="cluster-container"]:last-of-type '
                        'a[class*="see-more"]')

  def _DidLoadDocument(self, action_runner):
    action_runner.ScrollPage()
    action_runner.Wait(2)
    action_runner.ScrollPage()
    action_runner.Wait(2)
    action_runner.MouseClick(self._SEE_MORE_SELECTOR)
    action_runner.Wait(2)
    action_runner.ScrollPage()


class GooglePlayStoreDesktopStory(_MediaBrowsingStory):
  """ Navigate to the movies page of Google Play Store, scroll to the bottom,
  and click "see more" of a middle category (last before second scroll).
  """
  NAME = 'browse:media:googleplaystore:2021'
  URL = 'https://play.google.com/store/movies'
  ITEM_SELECTOR = ''
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  IS_SINGLE_PAGE_APP = True
  TAGS = [story_tags.YEAR_2018, story_tags.IMAGES]

  def _DidLoadDocument(self, action_runner):
    action_runner.ScrollPage()
    action_runner.Wait(2)
    action_runner.ScrollPage()
    action_runner.Wait(2)
    action_runner.ScrollPage()

  def __init__(self, story_set, take_memory_measurement,
               extra_browser_args=None, name_suffix=''):
    self.NAME = self.NAME + name_suffix
    super(GooglePlayStoreDesktopStory, self).__init__(story_set,
      take_memory_measurement,
      extra_browser_args=extra_browser_args)



##############################################################################
# Emerging market browsing stories.
##############################################################################


class BrowseFlipKartMobileStory2019(_ArticleBrowsingStory):
  NAME = 'browse:shopping:flipkart:2019'
  URL = 'https://flipkart.com/search?q=Sunglasses'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.EMERGING_MARKET, story_tags.YEAR_2019]

  ITEM_SELECTOR = '.r-1hvjb8t'
  BACK_SELECTOR = '._3NH1qf'
  ITEMS_TO_VISIT = 4

  def _WaitForNavigation(self, action_runner):
    action_runner.WaitForElement(text='View Details')

  def _NavigateBack(self, action_runner):
    action_runner.ClickElement(selector=self.BACK_SELECTOR)
    action_runner.WaitForElement(text="sunglasses")


class BrowseAmazonMobileStory2019(_ArticleBrowsingStory):
  NAME = 'browse:shopping:amazon:2019'
  URL = 'https://www.amazon.com.br/s/?k=telefone+celular'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.EMERGING_MARKET, story_tags.YEAR_2019]

  ITEM_SELECTOR = '[class="a-size-base a-color-base a-text-normal"]'
  ITEMS_TO_VISIT = 4


class BrowseLazadaMobileStory2019(_ArticleBrowsingStory):
  NAME = 'browse:shopping:lazada:2019'
  URL = 'https://www.lazada.co.id/catalog/?q=Wrist+watch'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.EMERGING_MARKET, story_tags.YEAR_2019]

  ITEM_SELECTOR = '.c12p0m'
  ITEMS_TO_VISIT = 4


class BrowseAvitoMobileStory2019(_ArticleBrowsingStory):
  NAME = 'browse:shopping:avito:2019'
  URL = 'https://www.avito.ru/rossiya'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.EMERGING_MARKET, story_tags.YEAR_2019]

  ITEM_SELECTOR = '._3eXe2'
  ITEMS_TO_VISIT = 4

  def _WaitForNavigation(self, action_runner):
    action_runner.WaitForElement(selector='[class="_3uGvV YVMFh"]')


class BrowseTOIMobileStory2019(_ArticleBrowsingStory):
  NAME = 'browse:news:toi:2019'
  URL = 'http://m.timesofindia.com'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.EMERGING_MARKET, story_tags.YEAR_2019]

  ITEMS_TO_VISIT = 2
  ITEM_SELECTOR = '.dummy-img'


class BrowseGloboMobileStory2019(_ArticleBrowsingStory):
  NAME = 'browse:news:globo:2019'
  URL = 'http://www.globo.com'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.EMERGING_MARKET, story_tags.YEAR_2019]
  ITEMS_TO_VISIT = 2  # 4 links causes renderer OOM crbug.com/714650.
  ITEM_SELECTOR = '.hui-premium__link'
  COMPLETE_STATE_WAIT_TIMEOUT = 150


class BrowseCricBuzzMobileStory2019(_ArticleBrowsingStory):
  NAME = 'browse:news:cricbuzz:2019'
  URL = 'http://m.cricbuzz.com'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.EMERGING_MARKET, story_tags.YEAR_2019]

  ITEMS_TO_VISIT = 3
  ITEM_SELECTOR = '.list-content'


##############################################################################
# Maps browsing stories.
##############################################################################


class GoogleMapsMobileStory2019(system_health_story.SystemHealthStory):
  """Story that browses google maps mobile page

  This story searches for nearby restaurants on google maps website and finds
  directions to a chosen restaurant from search results.
  """
  NAME = 'browse:tools:maps:2019'
  URL = 'https://maps.google.com/maps?force=pwa&source=mlpwa'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [
      story_tags.HEALTH_CHECK, story_tags.EMERGING_MARKET, story_tags.YEAR_2019
  ]

  _MAPS_SEARCH_BOX_SELECTOR = '.ml-searchbox-pwa-textarea'
  _MAPS_SEARCH_BOX_FORM = '[id="ml-searchboxform"]'
  _RESTAURANTS_LOADED = '.ml-panes-categorical-bottom-bar button'
  _RESTAURANTS_LINK = '.ml-entity-list-item-info'
  _DIRECTIONS_LINK = ('.ml-panes-entity-bottom-bar.'
                      'ml-panes-entity-bottom-bar-expanded '
                      'button[class$="last"]')
  _DIRECTIONS_LOADED = ('.ml-panes-directions-bottom-bar.'
                        'ml-panes-directions-bottom-bar-collapsed '
                        'button[class$="last"]')
  _MAP_LAYER = '.ml-map'

  def _DidLoadDocument(self, action_runner):
    # Submit search query.
    self._ClickLink(self._MAPS_SEARCH_BOX_SELECTOR, action_runner)
    action_runner.WaitForElement(selector=self._MAPS_SEARCH_BOX_FORM)
    action_runner.Wait(1) # Waiting to type
    action_runner.EnterText('restaurants near me')
    action_runner.PressKey('Return')
    action_runner.WaitForElement(selector=self._RESTAURANTS_LOADED)
    action_runner.WaitForNetworkQuiescence()
    action_runner.Wait(4) # User looking at restaurants

    # Open the restaurant list and select the first.
    self._ClickLink(self._RESTAURANTS_LOADED, action_runner)
    action_runner.WaitForElement(selector=self._RESTAURANTS_LINK)
    action_runner.Wait(3) # User reads about restaurant
    self._ClickLink(self._RESTAURANTS_LINK, action_runner)
    action_runner.Wait(1) # Reading description

    # Open directions to the restaurant from Google.
    self._ClickLink(self._DIRECTIONS_LINK, action_runner)
    action_runner.Wait(0.5)
    action_runner.EnterText('Google UK')
    action_runner.PressKey('Return')
    action_runner.WaitForElement(selector=self._DIRECTIONS_LOADED)
    action_runner.WaitForNetworkQuiescence()
    action_runner.Wait(2) # Seeing direction

  def _ClickLink(self, selector, action_runner):
    action_runner.WaitForElement(selector=selector)
    action_runner.ClickElement(selector=selector)


class GoogleMapsStory2019(_BrowsingStory):
  """
  Google maps story:
    _ Start at https://www.maps.google.com/maps
    _ Search for "restaurents near me" and wait for 4 sec.
    _ Click ZoomIn two times, waiting for 3 sec in between.
    _ Scroll the map horizontally and vertically.
    _ Pick a restaurant and ask for directions.
  """
  # When recording this story:
  # Force tactile using this: http://google.com/maps?force=tt
  # Force webgl using this: http://google.com/maps?force=webgl
  # Reduce the speed as mentioned in the comment below for
  # RepeatableBrowserDrivenScroll
  NAME = 'browse:tools:maps:2019'
  URL = 'https://www.google.com/maps'
  _MAPS_SEARCH_BOX_SELECTOR = '#searchboxinput'
  _MAPS_UPDATE_RESULTS_SELECTOR = '#section-query-on-pan-checkbox-id'
  _MAPS_ZOOM_IN_SELECTOR = '#widget-zoom-in'
  _MAPS_ZOOM_OUT_SELECTOR = '#widget-zoom-out'
  _RESTAURANTS_LOADED = ('[class="searchbox searchbox-shadow noprint '
                         'clear-button-shown"]')
  _RESTAURANT_LINK = '[data-result-index="1"]'
  _DIRECTIONS_LINK = '.section-action-chip-button[data-value=Directions]'
  _DIRECTIONS_FROM_BOX = '[class="tactile-searchbox-input"]'
  _DIRECTIONS_LOADED = '[class="section-directions-trip clearfix selected"]'
  # Get the current server response hash and store it for use
  # in _CHECK_RESTAURANTS_UPDATED.
  _GET_RESTAURANT_RESPONSE_HASH = '''
    document.querySelector({{restaurant_link}}).textContent
  '''
  # Check if the current restaurant server response hash is different from
  # the old one to checks that restaurants started to update. Also wait for
  # the completion of the loading by waiting for the button to change to loaded.
  # The response hash gets updated when we scroll or zoom since server provides
  # a new response for the updated locations with a new hash value.
  _CHECK_RESTAURANTS_UPDATED = '''
    (document.querySelector({{restaurant_link}}).textContent
        != {{ old_restaurant }})
    && (document.querySelector(
          '[class="searchbox searchbox-shadow noprint clear-button-shown"]')
          != null)
    '''
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.JAVASCRIPT_HEAVY, story_tags.WEBGL,
          story_tags.YEAR_2019]

  def _DidLoadDocument(self, action_runner):
    # Click on the search box.
    action_runner.WaitForElement(selector=self._MAPS_SEARCH_BOX_SELECTOR)
    action_runner.WaitForElement(selector=self._MAPS_ZOOM_IN_SELECTOR)

    # Submit search query.
    action_runner.ClickElement(selector=self._MAPS_SEARCH_BOX_SELECTOR)
    action_runner.EnterText('restaurants near me')
    action_runner.PressKey('Return')
    action_runner.WaitForElement(selector=self._RESTAURANTS_LOADED)
    action_runner.Wait(1)

    # Enable updating the results on map move/zoom.
    action_runner.WaitForElement(selector=self._MAPS_UPDATE_RESULTS_SELECTOR)
    action_runner.ClickElement(selector=self._MAPS_UPDATE_RESULTS_SELECTOR)

    # ZoomIn two times.
    prev_restaurant_hash = action_runner.EvaluateJavaScript(
        self._GET_RESTAURANT_RESPONSE_HASH,
        restaurant_link=self._RESTAURANT_LINK)
    action_runner.ClickElement(selector=self._MAPS_ZOOM_IN_SELECTOR)
    action_runner.Wait(0.5)
    action_runner.ClickElement(selector=self._MAPS_ZOOM_IN_SELECTOR)
    action_runner.Wait(0.5)
    action_runner.ClickElement(selector=self._MAPS_ZOOM_IN_SELECTOR)
    action_runner.WaitForJavaScriptCondition(
        self._CHECK_RESTAURANTS_UPDATED, restaurant_link=self._RESTAURANT_LINK,
        old_restaurant=prev_restaurant_hash, timeout=90)
    # This wait is required to fetch the data for all the tiles in the map.
    action_runner.Wait(1)

    prev_restaurant_hash = action_runner.EvaluateJavaScript(
        self._GET_RESTAURANT_RESPONSE_HASH,
        restaurant_link=self._RESTAURANT_LINK)
    action_runner.ClickElement(selector=self._MAPS_ZOOM_OUT_SELECTOR)
    action_runner.Wait(0.5)
    action_runner.ClickElement(selector=self._MAPS_ZOOM_OUT_SELECTOR)
    action_runner.WaitForJavaScriptCondition(
        self._CHECK_RESTAURANTS_UPDATED, restaurant_link=self._RESTAURANT_LINK,
        old_restaurant=prev_restaurant_hash, timeout=90)
    # This wait is required to fetch the data for all the tiles in the map.
    action_runner.Wait(1)

    # Reduce the speed (the current wpr is recorded with speed set to 50)  when
    # recording the wpr. If we scroll too fast, the data will not be recorded
    # well. After recording reset it back to the original value to have a more
    # realistic scroll.
    prev_restaurant_hash = action_runner.EvaluateJavaScript(
        self._GET_RESTAURANT_RESPONSE_HASH,
        restaurant_link=self._RESTAURANT_LINK)
    action_runner.RepeatableBrowserDrivenScroll(
        x_scroll_distance_ratio=0.0, y_scroll_distance_ratio=0.5,
        repeat_count=2, speed=500, timeout=120, repeat_delay_ms=2000)
    action_runner.WaitForJavaScriptCondition(
        self._CHECK_RESTAURANTS_UPDATED, restaurant_link=self._RESTAURANT_LINK,
        old_restaurant=prev_restaurant_hash, timeout=90)

    prev_restaurant_hash = action_runner.EvaluateJavaScript(
        self._GET_RESTAURANT_RESPONSE_HASH,
        restaurant_link=self._RESTAURANT_LINK)
    action_runner.RepeatableBrowserDrivenScroll(
        x_scroll_distance_ratio=0.5, y_scroll_distance_ratio=0,
        repeat_count=2, speed=500, timeout=120, repeat_delay_ms=2000)
    action_runner.WaitForJavaScriptCondition(
        self._CHECK_RESTAURANTS_UPDATED, restaurant_link=self._RESTAURANT_LINK,
        old_restaurant=prev_restaurant_hash, timeout=90)

    # To make the recording more realistic.
    action_runner.Wait(1)
    action_runner.ClickElement(selector=self._RESTAURANT_LINK)
    # To make the recording more realistic.
    action_runner.Wait(1)
    action_runner.WaitForElement(selector=self._DIRECTIONS_LINK)
    action_runner.ClickElement(selector=self._DIRECTIONS_LINK)
    action_runner.ClickElement(selector=self._DIRECTIONS_FROM_BOX)
    action_runner.EnterText('6 Pancras Road London')
    action_runner.PressKey('Return')
    action_runner.WaitForElement(selector=self._DIRECTIONS_LOADED)
    action_runner.Wait(1)

    # Cycle trough the travel modes.
    for travel_mode in [0,1,2,3,4,6]:
      selector = 'div[data-travel_mode="%s"]' % travel_mode
      action_runner.WaitForElement(selector=selector)
      action_runner.ClickElement(selector=selector)
      action_runner.Wait(3)


##############################################################################
# Gmail browsing stories.
##############################################################################
class _GmailBrowsingStory(system_health_story.SystemHealthStory):
  """Abstract base class for Gmail browsing stories.

  Adds common functionality for re-mapping + waiting on performance
  mark + measure data.
  """
  SKIP_LOGIN = False

  USE_LOGINURL = False

  # Patch performance.mark and measure to get notified about page events.
  PERFOMANCE_MARK_AND_MEASURE = '''
    window.__telemetry_observed_page_events = new Set();
    (function () {
      let reported_events = window.__telemetry_reported_page_events;
      let reported_measures = window.__telemetry_reported_page_measures;

      let observed = window.__telemetry_observed_page_events;
      let performance_mark = window.performance.mark;
      let performance_measure = window.performance.measure;

      window.performance.measure = function(label, mark) {
        performance_measure.call(window.performance, label, mark);
        if(reported_measures.hasOwnProperty(label)) {
          performance_mark.call(window.performance, reported_measures[label]);
          observed.add(reported_measures[label]);
        }
      }

      window.performance.mark = function (label) {
        performance_mark.call(window.performance, label);
        if (reported_events.hasOwnProperty(label)) {
          performance_mark.call(
              window.performance, reported_events[label]);
          observed.add(reported_events[label]);
        }
      }
    })();
  '''

  def __init__(self, story_set, take_memory_measurement,
               events_and_measures_reported):
    super(_GmailBrowsingStory,
          self).__init__(story_set, take_memory_measurement,
                         '--allow-browser-signin=false')
    self.script_to_evaluate_on_commit = js_template.Render(
        '''{{@events_and_measures_reported_by_page}}
        {{@performance_mark_and_measure}}''',
        events_and_measures_reported_by_page=events_and_measures_reported,
        performance_mark_and_measure=self.PERFOMANCE_MARK_AND_MEASURE)

  def _Login(self, action_runner):
    if self.USE_LOGINURL or self.wpr_mode in [
        wpr_modes.WPR_OFF, wpr_modes.WPR_RECORD
    ]:
      google_login.LoginWithLoginUrl(action_runner, self.URL)
    else:
      google_login.NewLoginGoogleAccount(action_runner, 'googletest')

      # Navigating to http://mail.google.com immediately leads to an infinite
      # redirection loop due to a bug in WPR (see
      # https://bugs.chromium.org/p/chromium/issues/detail?id=1036791). We
      # therefore first navigate to a dummy sub-URL to set up the session and
      # hit the resulting redirection loop. Afterwards, we can safely navigate
      # to http://mail.google.com.
      action_runner.tab.WaitForDocumentReadyStateToBeComplete()
      action_runner.Navigate(
          'https://mail.google.com/mail/mu/mp/872/trigger_redirection_loop')
      action_runner.tab.WaitForDocumentReadyStateToBeComplete()


class GmailLabelClickStory2020(_GmailBrowsingStory):
  NAME = 'browse:tools:gmail-labelclick:2020'
  # Needs to be http and not https.
  URL = 'http://mail.google.com/'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2020]

  _IMPORTANT_SELECTOR = 'div[data-tooltip="Important"]'

  # Page event queries.
  LABEL_CLICK_BEGIN_EVENT = '''
    (window.__telemetry_observed_page_events.has(
        "telemetry:reported_by_page:benchmark_begin"))
  '''
  LABEL_CLICK_END_EVENT = '''
    (window.__telemetry_observed_page_events.has(
        "telemetry:reported_by_page:benchmark_end"))
  '''

  # These maps translates page-specific event names to event names needed for
  # the reported_by_page:* metric.
  EVENTS_AND_MEASURES_REPORTED_BY_PAGE = '''
    window.__telemetry_reported_page_events = {
      'mail:lc-1':
          'telemetry:reported_by_page:benchmark_begin',
    };

    window.__telemetry_reported_page_measures = {
      'mail:lc':
          'telemetry:reported_by_page:benchmark_end'
    };
  '''

  def __init__(self, story_set, take_memory_measurement):
    super(GmailLabelClickStory2020,
          self).__init__(story_set, take_memory_measurement,
                         self.EVENTS_AND_MEASURES_REPORTED_BY_PAGE)

  def _DidLoadDocument(self, action_runner):
    action_runner.Wait(1)
    action_runner.EvaluateJavaScript(
        "document.evaluate(\"//span[text()='More']\", "
        "document, null, XPathResult.FIRST_ORDERED_NODE_TYPE, null)"
        ".singleNodeValue.click();"
    )
    action_runner.WaitForElement(selector=self._IMPORTANT_SELECTOR)
    action_runner.ClickElement(selector=self._IMPORTANT_SELECTOR)
    action_runner.WaitForJavaScriptCondition(self.LABEL_CLICK_BEGIN_EVENT)
    action_runner.WaitForJavaScriptCondition(self.LABEL_CLICK_END_EVENT)


class GmailOpenConversationStory2020(_GmailBrowsingStory):
  NAME = 'browse:tools:gmail-openconversation:2020'
  # Needs to be http and not https.
  URL = 'http://mail.google.com/'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2020]

  _CONV_SELECTOR = 'span[data-thread-id]'

  # Page event queries.
  OPEN_CONVERSATION_BEGIN_EVENT = '''
    (window.__telemetry_observed_page_events.has(
        "telemetry:reported_by_page:benchmark_begin"))
  '''
  OPEN_CONVERSATION_END_EVENT = '''
    (window.__telemetry_observed_page_events.has(
        "telemetry:reported_by_page:benchmark_end"))
  '''

  # These maps translates page-specific event names to event names needed for
  # the reported_by_page:* metric.
  EVENTS_AND_MEASURES_REPORTED_BY_PAGE = '''
    window.__telemetry_reported_page_events = {
      'mail:o-1':
          'telemetry:reported_by_page:benchmark_begin',
    };

    window.__telemetry_reported_page_measures = {
      'mail:o':
          'telemetry:reported_by_page:benchmark_end'
    };
  '''

  def __init__(self, story_set, take_memory_measurement):
    super(GmailOpenConversationStory2020,
          self).__init__(story_set, take_memory_measurement,
                         self.EVENTS_AND_MEASURES_REPORTED_BY_PAGE)

  def _DidLoadDocument(self, action_runner):
    action_runner.Wait(1)
    action_runner.ClickElement(selector=self._CONV_SELECTOR)
    action_runner.WaitForJavaScriptCondition(self.OPEN_CONVERSATION_BEGIN_EVENT)
    action_runner.WaitForJavaScriptCondition(self.OPEN_CONVERSATION_END_EVENT)

class GmailSearchStory2020(_GmailBrowsingStory):
  NAME = 'browse:tools:gmail-search:2020'
  # Needs to be http and not https.
  URL = 'http://mail.google.com/'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2020]

  _SEARCH_SELECTOR = 'input[aria-label="Search mail"]'

  # Page event queries.
  SEARCH_BEGIN_EVENT = '''
    (window.__telemetry_observed_page_events.has(
        "telemetry:reported_by_page:benchmark_begin"))
  '''
  SEARCH_END_EVENT = '''
    (window.__telemetry_observed_page_events.has(
        "telemetry:reported_by_page:benchmark_end"))
  '''

  # These maps translates page-specific event names to event names needed for
  # the reported_by_page:* metric.
  EVENTS_AND_MEASURES_REPORTED_BY_PAGE = '''
    window.__telemetry_reported_page_events = {
      'mail:se-1':
          'telemetry:reported_by_page:benchmark_begin',
    };

    window.__telemetry_reported_page_measures = {
      'mail:se':
          'telemetry:reported_by_page:benchmark_end'
    };
  '''

  def __init__(self, story_set, take_memory_measurement):
    super(GmailSearchStory2020,
          self).__init__(story_set, take_memory_measurement,
                         self.EVENTS_AND_MEASURES_REPORTED_BY_PAGE)

  def _DidLoadDocument(self, action_runner):
    action_runner.Wait(1)
    action_runner.ExecuteJavaScript(
        'document.querySelector({{ selector }}).focus()',
        selector=self._SEARCH_SELECTOR)
    action_runner.EnterText('deals')
    action_runner.PressKey('Return')
    action_runner.WaitForJavaScriptCondition(self.SEARCH_BEGIN_EVENT)
    action_runner.WaitForJavaScriptCondition(self.SEARCH_END_EVENT)

class GmailComposeStory2020(_GmailBrowsingStory):
  NAME = 'browse:tools:gmail-compose:2020'
  # Needs to be http and not https.
  URL = 'http://mail.google.com/'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2020]

  # Page event queries.
  COMPOSE_BEGIN_EVENT = '''
    (window.__telemetry_observed_page_events.has(
        "telemetry:reported_by_page:benchmark_begin"))
  '''
  COMPOSE_END_EVENT = '''
    (window.__telemetry_observed_page_events.has(
        "telemetry:reported_by_page:benchmark_end"))
  '''

  # These maps translates page-specific event names to event names needed for
  # the reported_by_page:* metric.
  EVENTS_AND_MEASURES_REPORTED_BY_PAGE = '''
    window.__telemetry_reported_page_events = {
      'mail:ob-1':
          'telemetry:reported_by_page:benchmark_begin',
    };

    window.__telemetry_reported_page_measures = {
      'mail:ob':
          'telemetry:reported_by_page:benchmark_end'
    };
  '''

  def __init__(self, story_set, take_memory_measurement):
    super(GmailComposeStory2020,
          self).__init__(story_set, take_memory_measurement,
                         self.EVENTS_AND_MEASURES_REPORTED_BY_PAGE)

  def _DidLoadDocument(self, action_runner):
    action_runner.Wait(1)
    action_runner.EvaluateJavaScript(
        "document.evaluate(\"//div[text()='Compose']\", document, "
        "null, XPathResult.FIRST_ORDERED_NODE_TYPE, null)"
        ".singleNodeValue.focus();")
    action_runner.PressKey('Return')

    action_runner.WaitForJavaScriptCondition(self.COMPOSE_BEGIN_EVENT)
    action_runner.WaitForJavaScriptCondition(self.COMPOSE_END_EVENT)

##############################################################################
# Google sheets browsing story.
##############################################################################


class GoogleSheetsDesktopStory(system_health_story.SystemHealthStory):
  NAME = 'browse:tools:sheets:2019'
  URL = ('https://docs.google.com/spreadsheets/d/' +
         '16jfsJs14QrWKhsbxpdJXgoYumxNpnDt08DTK82Puc2A/' +
         'edit#gid=896027318&range=C:C')
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [
      story_tags.HEALTH_CHECK, story_tags.JAVASCRIPT_HEAVY, story_tags.YEAR_2019
  ]

  # This map translates page-specific event names to event names needed for
  # the reported_by_page:* metric.
  EVENTS_REPORTED_BY_PAGE = '''
    window.__telemetry_reported_page_events = {
      'ccv':
          'telemetry:reported_by_page:viewable',
      'fcoe':
          'telemetry:reported_by_page:interactive',
      'first_meaningful_calc_begin':
          'telemetry:reported_by_page:benchmark_begin',
      'first_meaningful_calc_end':
          'telemetry:reported_by_page:benchmark_end'
    };
  '''

  # Patch performance.mark to get notified about page events.
  PERFOMANCE_MARK = '''
    window.__telemetry_observed_page_events = new Set();
    (function () {
      let reported = window.__telemetry_reported_page_events;
      let observed = window.__telemetry_observed_page_events;
      let performance_mark = window.performance.mark;
      window.performance.mark = function (label) {
        performance_mark.call(window.performance, label);
        if (reported.hasOwnProperty(label)) {
          performance_mark.call(
              window.performance, reported[label]);
          observed.add(reported[label]);
        }
      }
    })();
  '''

  # Page event queries.
  INTERACTIVE_EVENT = '''
    (window.__telemetry_observed_page_events.has(
        "telemetry:reported_by_page:interactive"))
  '''
  CALC_BEGIN_EVENT = '''
    (window.__telemetry_observed_page_events.has(
        "telemetry:reported_by_page:benchmark_begin"))
  '''
  CALC_END_EVENT = '''
    (window.__telemetry_observed_page_events.has(
        "telemetry:reported_by_page:benchmark_end"))
  '''
  CLEAR_EVENTS = 'window.__telemetry_observed_page_events.clear()'

  # Start recalculation of the current column by invoking
  # trixApp.module$contents$waffle$DesktopRitzApp_DesktopRitzApp$actionRegistry_
  #   .f_actions__com_google_apps_docs_xplat_controller_ActionRegistryImpl_
  #   ['trix-fill-right'].fireAction()
  RECALCULATE_COLUMN = "trixApp.Cb.D['trix-fill-right'].Eb();"

  # Patch the goog$Uri.prototype.setParameterValue to fix the
  # session parameters that depend on Math.random and Date.now.
  DETERMINISITIC_SESSION = '''
    if (window.xk) {
      window.xk.prototype.wc = function (a, b) {
          if (a === "rand") { b = "1566829321650"; }
          if (a === "zx") { b = "9azccr4i1bz5"; }
          if (a === "ssfi") { b = "0"; }
          this.C.set(a, b);
          return this;
      }
    }
  '''

  def __init__(self, story_set, take_memory_measurement):
    super(GoogleSheetsDesktopStory, self).__init__(story_set,
                                                   take_memory_measurement)
    self.script_to_evaluate_on_commit = js_template.Render(
        '''{{@events_reported_by_page}}
        {{@performance_mark}}
        document.addEventListener('readystatechange', event => {
          if (event.target.readyState === 'interactive') {
            {{@deterministic_session}}
          }
        });''',
        events_reported_by_page=self.EVENTS_REPORTED_BY_PAGE,
        performance_mark=self.PERFOMANCE_MARK,
        deterministic_session=self.DETERMINISITIC_SESSION)

  def _DidLoadDocument(self, action_runner):
    # 1. Wait until the spreadsheet loads.
    action_runner.WaitForJavaScriptCondition(self.INTERACTIVE_EVENT)
    # 2. Idle for 5 seconds for Chrome's timeToInteractive metric.
    action_runner.Wait(5)
    # 3. Prepare for observing calcuation events.
    action_runner.EvaluateJavaScript(self.CLEAR_EVENTS)
    # 4. Recalculate the column.
    action_runner.EvaluateJavaScript(self.RECALCULATE_COLUMN)
    # 5. Wait for calculation completion.
    action_runner.WaitForJavaScriptCondition(self.CALC_BEGIN_EVENT)
    action_runner.WaitForJavaScriptCondition(self.CALC_END_EVENT)


##############################################################################
# Google docs browsing stories.
##############################################################################


class GoogleDocsDesktopScrollingStory(system_health_story.SystemHealthStory):
  """
  Google Docs scrolling story:
    _ Open a document
    _ Wait for UI to become available
    _ Scroll through the document
  """
  NAME = 'browse:tools:docs_scrolling'
  # Test document. safe=true forces synchronous layout which helps determinism
  # pylint: disable=line-too-long
  URL = 'https://docs.google.com/document/d/14sZMXhI1NljEDSFfxhgA4FNI2_rSImxx3tZ4YsNRUdU/preview?safe=true&Debug=true'
  # pylint: enable=line-too-long
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.JAVASCRIPT_HEAVY, story_tags.YEAR_2020]

  # This map translates page-specific event names to event names needed for
  # the reported_by_page:* metric.
  EVENTS_REPORTED_BY_PAGE = '''
    window.__telemetry_reported_page_events = {
      'ccv':
          'telemetry:reported_by_page:viewable',
      'fcoe':
          'telemetry:reported_by_page:interactive',
    };
  '''

  # Patch performance.mark to get notified about page events.
  PERFORMANCE_MARK_PATCH = '''
    window.__telemetry_observed_page_events = new Set();
    (function () {
      let reported = window.__telemetry_reported_page_events;
      let observed = window.__telemetry_observed_page_events;
      let performance_mark = window.performance.mark;
      window.performance.mark = function (label) {
        performance_mark.call(window.performance, label);
        if (reported.hasOwnProperty(label)) {
          performance_mark.call(
              window.performance, reported[label]);
          observed.add(reported[label]);
        }
      }
    })();
  '''

  # Page event queries.
  INTERACTIVE_EVENT = '''
    (window.__telemetry_observed_page_events.has(
        "telemetry:reported_by_page:interactive"))
  '''
  CLEAR_EVENTS = 'window.__telemetry_observed_page_events.clear()'

  def __init__(self, story_set, take_memory_measurement):
    super(GoogleDocsDesktopScrollingStory, self).__init__(
        story_set, take_memory_measurement)
    self.script_to_evaluate_on_commit = js_template.Render(
        '''{{@events_reported_by_page}}
        {{@performance_mark}}''',
        events_reported_by_page=self.EVENTS_REPORTED_BY_PAGE,
        performance_mark=self.PERFORMANCE_MARK_PATCH)

  def _DidLoadDocument(self, action_runner):
    # Wait for load.
    action_runner.WaitForJavaScriptCondition(self.INTERACTIVE_EVENT)
    action_runner.Wait(5)
    action_runner.EvaluateJavaScript(self.CLEAR_EVENTS)
    # Scroll through the document.
    action_runner.RepeatableBrowserDrivenScroll(
        x_scroll_distance_ratio=0.0,
        y_scroll_distance_ratio=1,
        repeat_count=6,
        speed=800,
        timeout=120)

##############################################################################
# Browsing stories with infinite scrolling
##############################################################################


class _InfiniteScrollStory(system_health_story.SystemHealthStory):
  SUPPORTED_PLATFORMS = platforms.ALL_PLATFORMS

  SCROLL_DISTANCE = 25000
  SCROLL_STEP = 1000
  MAX_SCROLL_RETRIES = 3
  TIME_TO_WAIT_BEFORE_STARTING_IN_SECONDS = 5

  def __init__(self,
               story_set,
               take_memory_measurement,
               extra_browser_args=None):
    super(_InfiniteScrollStory,
          self).__init__(story_set,
                         take_memory_measurement,
                         extra_browser_args=extra_browser_args)
    self.script_to_evaluate_on_commit = '''
        window.WebSocket = undefined;
        window.Worker = undefined;
        window.performance = undefined;''' + override_online.ALWAYS_ONLINE

  def _DidLoadDocument(self, action_runner):
    action_runner.WaitForJavaScriptCondition(
      'document.body != null && '
      'document.body.scrollHeight > window.innerHeight && '
      '!document.body.addEventListener("touchstart", function() {})')
    action_runner.Wait(self.TIME_TO_WAIT_BEFORE_STARTING_IN_SECONDS)
    self._Scroll(action_runner, self.SCROLL_DISTANCE, self.SCROLL_STEP)


  def _Scroll(self, action_runner, distance, step_size):
    """ This function scrolls the webpage by the given scroll distance in
    multiple steps, where each step (except the last one) has the given size.

    If scrolling gets stuck, the function waits for the page's scroll height to
    expand then retries scrolling.
    """
    remaining = distance - action_runner.EvaluateJavaScript('window.scrollY')
    # Scroll until the window.scrollY is within 1 pixel of the target distance.
    while remaining > 1:
      last_scroll_height = action_runner.EvaluateJavaScript(
          'document.body.scrollHeight')
      action_runner.ScrollPage(distance=min(remaining, step_size) + 1)
      new_remaining = (distance -
          action_runner.EvaluateJavaScript('window.scrollY'))
      if remaining <= new_remaining:
        # If the page contains an element with a scrollbar, then the synthetic
        # gesture generated by action_runner.ScrollPage might have scrolled that
        # element instead of the page. Retry scrolling at different place.
        # See https://crbug.com/884183.
        action_runner.ScrollPage(distance=min(remaining, step_size) + 1,
                                 left_start_ratio=0.01,
                                 top_start_ratio=0.01)
        new_remaining = (distance -
            action_runner.EvaluateJavaScript('window.scrollY'))

      if remaining <= new_remaining:
        # Scrolling is stuck. This can happen if the page is loading
        # resources. Wait for the page's scrollheight to expand and retry
        # scrolling.
        action_runner.WaitForJavaScriptCondition(
            'document.body.scrollHeight > {{ last_scroll_height }} ',
            last_scroll_height=last_scroll_height)
      else:
        remaining = new_remaining

  @classmethod
  def GenerateStoryDescription(cls):
    return 'Load %s then make a very long scroll.' % cls.URL


class DiscourseDesktopStory2018(_InfiniteScrollStory):
  NAME = 'browse:tech:discourse_infinite_scroll:2018'
  URL = 'https://meta.discourse.org/t/topic-list-previews/41630/28'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.INFINITE_SCROLL, story_tags.YEAR_2018]


class DiscourseMobileStory2018(_InfiniteScrollStory):
  NAME = 'browse:tech:discourse_infinite_scroll:2018'
  URL = 'https://meta.discourse.org/t/topic-list-previews/41630/28'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.INFINITE_SCROLL, story_tags.YEAR_2018]


class FacebookScrollDesktopStory2018(_InfiniteScrollStory):
  NAME = 'browse:social:facebook_infinite_scroll:2018'
  URL = 'https://www.facebook.com/shakira'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.INFINITE_SCROLL, story_tags.YEAR_2018]

  def _Login(self, action_runner):
    facebook_login.LoginWithDesktopSite(action_runner, 'facebook3')


class FacebookScrollMobileStory2018(_InfiniteScrollStory):
  NAME = 'browse:social:facebook_infinite_scroll:2018'
  URL = 'https://m.facebook.com/shakira'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.YEAR_2018]

  def _Login(self, action_runner):
    facebook_login.LoginWithMobileSite(action_runner, 'facebook3')


class FlickrMobileStory2019(_InfiniteScrollStory):
  NAME = 'browse:media:flickr_infinite_scroll:2019'
  URL = 'https://www.flickr.com/explore'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  SCROLL_DISTANCE = 10000
  TAGS = [story_tags.INFINITE_SCROLL, story_tags.YEAR_2019]


class PinterestMobileStory2021(_InfiniteScrollStory):
  NAME = 'browse:social:pinterest_infinite_scroll:2021'
  URL = 'https://www.pinterest.co.uk'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [
      story_tags.HEALTH_CHECK, story_tags.INFINITE_SCROLL, story_tags.YEAR_2021
  ]
  # TODO(crbug.com/40584277): Story breaks if login is skipped during replay.
  SKIP_LOGIN = False

  def _Login(self, action_runner):
    pinterest_login.LoginMobileAccount(action_runner, 'googletest')


class TumblrStory2018(_InfiniteScrollStory):
  NAME = 'browse:social:tumblr_infinite_scroll:2018'
  URL = 'https://techcrunch.tumblr.com/'
  SCROLL_DISTANCE = 20000
  TAGS = [
      story_tags.HEALTH_CHECK, story_tags.INFINITE_SCROLL,
      story_tags.JAVASCRIPT_HEAVY, story_tags.YEAR_2018
  ]

  def __init__(self, story_set, take_memory_measurement):
    super(TumblrStory2018, self).__init__(story_set, take_memory_measurement)

  def _Login(self, action_runner):
    tumblr_login.LoginDesktopAccount(action_runner, 'tumblr')
    action_runner.Wait(5)
    # Without this page reload the mobile version does not correctly
    # go to the https://techcrunch.tumblr.com
    action_runner.ReloadPage()

class TwitterScrollDesktopStory2018(_InfiniteScrollStory):
  NAME = 'browse:social:twitter_infinite_scroll:2018'
  URL = 'https://twitter.com/NASA'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.INFINITE_SCROLL, story_tags.YEAR_2018]
