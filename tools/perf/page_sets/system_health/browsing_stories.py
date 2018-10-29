# encoding: utf-8
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


# The number of lines will be reduced after 2018 update is complete and
# the old stories are removed: https://crbug.com/878390.
# pylint: disable=too-many-lines


from page_sets.system_health import platforms
from page_sets.system_health import story_tags
from page_sets.system_health import system_health_story

from page_sets.login_helpers import facebook_login
from page_sets.login_helpers import pinterest_login
from page_sets.login_helpers import tumblr_login

from telemetry.util import js_template


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

  def _DidLoadDocument(self, action_runner):
    for i in xrange(self.ITEMS_TO_VISIT):
      self._NavigateToItem(action_runner, i)
      self._ReadNextArticle(action_runner)
      self._NavigateBack(action_runner)
      self._ScrollMainPage(action_runner)

  def _ReadNextArticle(self, action_runner):
    if self.COMPLETE_STATE_WAIT_TIMEOUT is not None:
      action_runner.tab.WaitForDocumentReadyStateToBeComplete(
          timeout=self.COMPLETE_STATE_WAIT_TIMEOUT)
    else:
      action_runner.tab.WaitForDocumentReadyStateToBeComplete()
    action_runner.Wait(self.ITEM_READ_TIME_IN_SECONDS/2.0)
    action_runner.RepeatableBrowserDrivenScroll(
        repeat_count=self.ITEM_SCROLL_REPEAT)
    action_runner.Wait(self.ITEM_READ_TIME_IN_SECONDS/2.0)

  def _ScrollMainPage(self, action_runner):
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()
    action_runner.RepeatableBrowserDrivenScroll(
        repeat_count=self.MAIN_PAGE_SCROLL_REPEAT)


##############################################################################
# News browsing stories.
##############################################################################


class CnnStory(_ArticleBrowsingStory):
  """The second top website in http://www.alexa.com/topsites/category/News"""
  NAME = 'browse:news:cnn'
  URL = 'http://edition.cnn.com/'
  ITEM_SELECTOR = '.cd__content > h3 > a'
  ITEMS_TO_VISIT = 2
  TAGS = [story_tags.JAVASCRIPT_HEAVY, story_tags.HEALTH_CHECK,
          story_tags.YEAR_2016]

class CnnStory2018(_ArticleBrowsingStory):
  """The second top website in http://www.alexa.com/topsites/category/News"""
  NAME = 'browse:news:cnn:2018'
  URL = 'http://edition.cnn.com/'
  ITEM_SELECTOR = '.cd__content > h3 > a'
  ITEMS_TO_VISIT = 2
  TAGS = [story_tags.JAVASCRIPT_HEAVY, story_tags.HEALTH_CHECK,
          story_tags.YEAR_2018]


class FacebookMobileStory(_ArticleBrowsingStory):
  NAME = 'browse:social:facebook'
  URL = 'https://www.facebook.com/rihanna'
  ITEM_SELECTOR = 'article ._5msj'
  # We scroll further than usual so that Facebook fetches enough items
  # (crbug.com/631022)
  MAIN_PAGE_SCROLL_REPEAT = 1
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.EMERGING_MARKET, story_tags.YEAR_2016]


class FacebookDesktopStory(_ArticleBrowsingStory):
  NAME = 'browse:social:facebook'
  URL = 'https://www.facebook.com/rihanna'
  ITEM_SELECTOR = '._4-eo'
  IS_SINGLE_PAGE_APP = True
  # Web-page-replay does not work for this website:
  # https://github.com/chromium/web-page-replay/issues/79.
  SUPPORTED_PLATFORMS = platforms.NO_PLATFORMS
  TAGS = [story_tags.YEAR_2016]


class InstagramMobileStory(_ArticleBrowsingStory):
  NAME = 'browse:social:instagram'
  URL = 'https://www.instagram.com/badgalriri/'
  ITEM_SELECTOR = '[class="_8mlbc _vbtk2 _t5r8b"]'
  ITEMS_TO_VISIT = 8

  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.EMERGING_MARKET, story_tags.YEAR_2016]

  def _WaitForNavigation(self, action_runner):
    action_runner.WaitForElement(text='load more comments')

  def _NavigateBack(self, action_runner):
    action_runner.NavigateBack()


class FlipboardDesktopStory(_ArticleBrowsingStory):
  NAME = 'browse:news:flipboard'
  URL = 'https://flipboard.com/explore'
  IS_SINGLE_PAGE_APP = True
  ITEM_SELECTOR = '.cover-image'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2016]


class FlipboardDesktopStory2018(_ArticleBrowsingStory):
  NAME = 'browse:news:flipboard:2018'
  URL = 'https://flipboard.com/explore'
  IS_SINGLE_PAGE_APP = True
  ITEM_SELECTOR = '.cover-image'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2018]


class HackerNewsDesktopStory(_ArticleBrowsingStory):
  NAME = 'browse:news:hackernews'
  URL = 'https://news.ycombinator.com'
  ITEM_SELECTOR = '.athing .title > a'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2016]


class HackerNewsDesktopStory2018(_ArticleBrowsingStory):
  NAME = 'browse:news:hackernews:2018'
  URL = 'https://news.ycombinator.com'
  ITEM_SELECTOR = '.athing .title > a'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2018]


class NytimesDesktopStory(_ArticleBrowsingStory):
  """The third top website in http://www.alexa.com/topsites/category/News"""
  NAME = 'browse:news:nytimes'
  URL = 'http://www.nytimes.com'
  ITEM_SELECTOR = '.story-heading > a'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  COMPLETE_STATE_WAIT_TIMEOUT = 150  # crbug.com/865247
  TAGS = [story_tags.YEAR_2016]


class NytimesDesktopStory2018(_ArticleBrowsingStory):
  """
  The third top website in http://www.alexa.com/topsites/category/News
  Known Replay Errors:
  - window.EventTracker is not loaded
  - all network errors are related to ads
  """
  NAME = 'browse:news:nytimes:2018'
  URL = 'http://www.nytimes.com'
  ITEM_SELECTOR = "a[href*='/2018/']"
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2018]


# Desktop qq.com opens a news item in a separate tab, for which the back button
# does not work.
class QqMobileStory(_ArticleBrowsingStory):
  NAME = 'browse:news:qq'
  URL = 'http://news.qq.com'
  ITEM_SELECTOR = '.list .full a'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.INTERNATIONAL, story_tags.HEALTH_CHECK,
          story_tags.YEAR_2016]


class RedditDesktopStory(_ArticleBrowsingStory):
  """The top website in http://www.alexa.com/topsites/category/News"""
  NAME = 'browse:news:reddit'
  URL = 'https://www.reddit.com/r/news/top/?sort=top&t=week'
  ITEM_SELECTOR = '.thing .title > a'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2016]


class RedditDesktopStory2018(_ArticleBrowsingStory):
  """The top website in http://www.alexa.com/topsites/category/News"""
  NAME = 'browse:news:reddit:2018'
  URL = 'https://www.reddit.com/r/news/top/?sort=top&t=week'
  IS_SINGLE_PAGE_APP = True
  ITEM_SELECTOR = 'article'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2018]


class RedditMobileStory(_ArticleBrowsingStory):
  """The top website in http://www.alexa.com/topsites/category/News"""
  NAME = 'browse:news:reddit'
  URL = 'https://www.reddit.com/r/news/top/?sort=top&t=week'
  IS_SINGLE_PAGE_APP = True
  ITEM_SELECTOR = '.PostHeader__post-title-line'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.YEAR_2016]


class TwitterMobileStory(_ArticleBrowsingStory):
  NAME = 'browse:social:twitter'
  URL = 'https://www.twitter.com/nasa'
  ITEM_SELECTOR = '.Tweet-text'
  CONTAINER_SELECTOR = '.NavigationSheet'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.YEAR_2016]


class TwitterDesktopStory(_ArticleBrowsingStory):
  NAME = 'browse:social:twitter'
  URL = 'https://www.twitter.com/nasa'
  IS_SINGLE_PAGE_APP = True
  ITEM_SELECTOR = '.tweet-text'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2016]


class TwitterDesktopStory2018(_ArticleBrowsingStory):
  NAME = 'browse:social:twitter:2018'
  URL = 'https://www.twitter.com/nasa'
  IS_SINGLE_PAGE_APP = True
  ITEM_SELECTOR = '.tweet-text'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2018]


class WashingtonPostMobileStory(_ArticleBrowsingStory):
  """Progressive website"""
  NAME = 'browse:news:washingtonpost'
  URL = 'https://www.washingtonpost.com/pwa'
  IS_SINGLE_PAGE_APP = True
  ITEM_SELECTOR = '.hed > a'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  _CLOSE_BUTTON_SELECTOR = '.close'
  TAGS = [story_tags.YEAR_2016]

  def _DidLoadDocument(self, action_runner):
    # Close the popup window. On Nexus 9 (and probably other tables) the popup
    # window does not have a "Close" button, instead it has only a "Send link
    # to phone" button. So on tablets we run with the popup window open. The
    # popup is transparent, so this is mostly an aesthetical issue.
    has_button = action_runner.EvaluateJavaScript(
        '!!document.querySelector({{ selector }})',
        selector=self._CLOSE_BUTTON_SELECTOR)
    if has_button:
      action_runner.ClickElement(selector=self._CLOSE_BUTTON_SELECTOR)
    super(WashingtonPostMobileStory, self)._DidLoadDocument(action_runner)


##############################################################################
# Search browsing stories.
##############################################################################


class GoogleDesktopStory(_ArticleBrowsingStory):
  """
  A typical google search story:
    _ Start at https://www.google.com/search?q=flower
    _ Click on the wikipedia link & navigate to
      https://en.wikipedia.org/wiki/Flower
    _ Scroll down the wikipedia page about flower.
    _ Back to the search main page.
    _ Refine the search query to 'flower delivery'.
    _ Scroll down the page.
    _ Click the next page result of 'flower delivery'.
    _ Scroll the search page.

  """
  NAME = 'browse:search:google'
  URL = 'https://www.google.com/search?q=flower'
  _SEARCH_BOX_SELECTOR = 'input[aria-label="Search"]'
  _SEARCH_PAGE_2_SELECTOR = 'a[aria-label="Page 2"]'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2016]

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
    action_runner.ClickElement(selector=self._SEARCH_BOX_SELECTOR)
    action_runner.Wait(2)

    # Submit search query.
    action_runner.EnterText(' delivery')
    action_runner.Wait(0.5)
    action_runner.PressKey('Return')

    # Scroll down & click next search result page.
    action_runner.Wait(2)
    action_runner.ScrollPageToElement(selector=self._SEARCH_PAGE_2_SELECTOR)
    action_runner.Wait(2)
    action_runner.ClickElement(selector=self._SEARCH_PAGE_2_SELECTOR)
    action_runner.Wait(2)
    action_runner.ScrollPage()

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
  NAME = 'browse:search:google:2018'
  URL = 'https://www.google.com/search?q=flower&hl=en'
  _SEARCH_BOX_SELECTOR = 'input[aria-label="Search"]'
  _SEARCH_PAGE_2_SELECTOR = 'a[aria-label="Page 2"]'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2018]

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


class GoogleIndiaDesktopStory(_ArticleBrowsingStory):
  """
  A typical google search story in India:
    1. Start at https://www.google.co.in/search?q=%E0%A4%AB%E0%A5%82%E0%A4%B2`
    2. Scroll down the page.
    3. Refine the query & click search box, which navigates to
    https://www.google.co.in/search?q=%E0%A4%AB%E0%A5%82%E0%A4%B2&rct=j#q=%E0%A4%AB%E0%A5%82%E0%A4%B2+%E0%A4%B5%E0%A4%BF%E0%A4%A4%E0%A4%B0%E0%A4%A3
    4. Scroll down the page.
    5. Click the next page result
    6. Scroll the search result page.

  """
  NAME = 'browse:search:google_india'
  URL = 'https://www.google.co.in/search?q=%E0%A4%AB%E0%A5%82%E0%A4%B2'
  _SEARCH_BOX_SELECTOR = 'input[aria-label="Search"]'
  _SEARCH_BUTTON_SELECTOR = 'button[aria-label="Google Search"]'
  _SEARCH_PAGE_2_SELECTOR = 'a[aria-label="Page 2"]'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.INTERNATIONAL, story_tags.YEAR_2016]

  def _DidLoadDocument(self, action_runner):
    action_runner.Wait(2)
    action_runner.ScrollPage()
    action_runner.Wait(2)

    action_runner.ScrollPage(direction='up')

    # Refine search query in the search box.
    # TODO(nednguyen): replace this with input text gesture to make it more
    # realistic.
    action_runner.ExecuteJavaScript(
        'document.querySelector({{ selector }}).value += "वितरण";',
        selector=self._SEARCH_BOX_SELECTOR)
    action_runner.Wait(2)
    action_runner.ClickElement(selector=self._SEARCH_BUTTON_SELECTOR)

    # Scroll down & click next search result page.
    action_runner.Wait(2)
    action_runner.ScrollPageToElement(selector=self._SEARCH_PAGE_2_SELECTOR)
    action_runner.Wait(2)
    action_runner.ClickElement(selector=self._SEARCH_PAGE_2_SELECTOR)
    action_runner.Wait(2)
    action_runner.ScrollPage()


class GoogleIndiaDesktopStory2018(_ArticleBrowsingStory):
  """
  A typical google search story in India:
    1. Start at self.URL
    2. Scroll down the page.
    3. Refine the query & click search box
    4. Scroll down the page.
    5. Click the next page result
    6. Scroll the search result page.

  """
  NAME = 'browse:search:google_india:2018'
  URL = 'https://www.google.co.in/search?q=%E0%A4%AB%E0%A5%82%E0%A4%B2&hl=hi'
  _SEARCH_BOX_SELECTOR = 'input[name="q"]'
  _SEARCH_BUTTON_SELECTOR = 'button[name="btnG"]'
  _SEARCH_PAGE_2_SELECTOR = 'a[aria-label="Page 2"]'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.INTERNATIONAL, story_tags.YEAR_2018]

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
    for _ in xrange(self.ITEMS_TO_VISIT):
      self._NavigateToItem(action_runner, index)
      self._ViewMediaItem(action_runner, index)
      if self.INCREMENT_INDEX_AFTER_EACH_ITEM:
        index += 1


  def _ViewMediaItem(self, action_runner, index):
    del index  # Unused.
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()
    action_runner.Wait(self.ITEM_VIEW_TIME_IN_SECONDS)


class ImgurMobileStory(_MediaBrowsingStory):
  NAME = 'browse:media:imgur'
  URL = 'http://imgur.com/gallery/5UlBN'
  ITEM_SELECTOR = '.Navbar-customAction'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  IS_SINGLE_PAGE_APP = True
  TAGS = [story_tags.EMERGING_MARKET, story_tags.YEAR_2016]


class ImgurDesktopStory(_MediaBrowsingStory):
  NAME = 'browse:media:imgur'
  URL = 'http://imgur.com/gallery/5UlBN'
  ITEM_SELECTOR = '.navNext'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  IS_SINGLE_PAGE_APP = True
  TAGS = [story_tags.YEAR_2016]


class YouTubeMobileStory(_MediaBrowsingStory):
  """Load a typical YouTube video then navigate to a next few videos. Stop and
  watch each video for few seconds.
  """
  NAME = 'browse:media:youtube'
  URL = 'https://m.youtube.com/watch?v=QGfhS1hfTWw&autoplay=false'
  ITEM_SELECTOR = '._mhgb > a'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  IS_SINGLE_PAGE_APP = True
  ITEM_SELECTOR_INDEX = 3
  TAGS = [story_tags.JAVASCRIPT_HEAVY, story_tags.EMERGING_MARKET,
          story_tags.HEALTH_CHECK, story_tags.YEAR_2016]


class YouTubeDesktopStory(_MediaBrowsingStory):
  """Load a typical YouTube video then navigate to a next few videos. Stop and
  watch each video for a few seconds.
  """
  NAME = 'browse:media:youtube'
  URL = 'https://www.youtube.com/watch?v=QGfhS1hfTWw&autoplay=false'
  ITEM_SELECTOR = '.yt-uix-simple-thumb-related'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  IS_SINGLE_PAGE_APP = True
  # A longer view time allows videos to load and play.
  ITEM_VIEW_TIME_IN_SECONDS = 5
  ITEMS_TO_VISIT = 8
  ITEM_SELECTOR_INDEX = 3
  PLATFORM_SPECIFIC = True
  TAGS = [story_tags.JAVASCRIPT_HEAVY, story_tags.YEAR_2016]


class FacebookPhotosMobileStory(_MediaBrowsingStory):
  """Load a photo page from Rihanna's facebook page then navigate a few next
  photos.
  """
  NAME = 'browse:media:facebook_photos'
  URL = (
      'https://m.facebook.com/rihanna/photos/a.207477806675.138795.10092511675/10153911739606676/?type=3&source=54&ref=page_internal')
  ITEM_SELECTOR = '._57-r.touchable'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  IS_SINGLE_PAGE_APP = True
  ITEM_SELECTOR_INDEX = 0
  TAGS = [story_tags.EMERGING_MARKET, story_tags.YEAR_2016]


class FacebookPhotosDesktopStory(_MediaBrowsingStory):
  """Load a photo page from Rihanna's facebook page then navigate a few next
  photos.
  """
  NAME = 'browse:media:facebook_photos'
  URL = (
      'https://www.facebook.com/rihanna/photos/a.207477806675.138795.10092511675/10153911739606676/?type=3&theater')
  ITEM_SELECTOR = '.snowliftPager.next'
  # Recording currently does not work. The page gets stuck in the
  # theater viewer.
  SUPPORTED_PLATFORMS = platforms.NO_PLATFORMS
  IS_SINGLE_PAGE_APP = True
  TAGS = [story_tags.YEAR_2016]


class TumblrDesktopStory(_MediaBrowsingStory):
  NAME = 'browse:media:tumblr'
  URL = 'https://tumblr.com/search/gifs'
  ITEM_SELECTOR = '.photo'
  IS_SINGLE_PAGE_APP = True
  ITEMS_TO_VISIT = 8
  INCREMENT_INDEX_AFTER_EACH_ITEM = True
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2016]

  def _ViewMediaItem(self, action_runner, index):
    super(TumblrDesktopStory, self)._ViewMediaItem(action_runner, index)
    action_runner.Wait(5)  # Give the lightbox time to appear
    action_runner.MouseClick(selector='#tumblr_lightbox_center_image')
    action_runner.Wait(1)  # To make browsing more realistic.


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


class PinterestDesktopStory(_MediaBrowsingStory):
  NAME = 'browse:media:pinterest'
  URL = 'https://pinterest.com'
  ITEM_SELECTOR = '.pinImageDim'
  IS_SINGLE_PAGE_APP = True
  ITEMS_TO_VISIT = 8
  INCREMENT_INDEX_AFTER_EACH_ITEM = True
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.YEAR_2016]

  def _Login(self, action_runner):
    pinterest_login.LoginDesktopAccount(action_runner, 'googletest')

  def _ViewMediaItem(self, action_runner, index):
    super(PinterestDesktopStory, self)._ViewMediaItem(action_runner, index)
    # To imitate real user interaction, we do not want to pin every post.
    # We will only pin every other post.
    if index % 2 == 0:
      # Pin the selection.
      save_function = ('document.querySelector('
                       '".Button.Module.ShowModalButton.btn.hasIcon.hasText.'
                       'isBrioFlat.medium.primary.primaryOnHover.repin.'
                       'pinActionBarButton.isBrioFlat.rounded")')
      action_runner.ClickElement(element_function=save_function)
      action_runner.Wait(1)  # Wait to make navigation realistic.
      # Select which board to pin to.
      inner_save_function = 'document.querySelector(".nameAndIcons")'
      action_runner.WaitForElement(element_function=inner_save_function)
      action_runner.ClickElement(element_function=inner_save_function)
      action_runner.Wait(1)  # Wait to make navigation realistic.

    # Close selection.
    x_element_function = ('document.querySelector('
                          '".Button.borderless.close.visible")')
    action_runner.ClickElement(element_function=x_element_function)
    action_runner.Wait(1)  # Wait to make navigation realistic.


##############################################################################
# Emerging market browsing stories.
##############################################################################


class BrowseFlipKartMobileStory(_ArticleBrowsingStory):
  NAME = 'browse:shopping:flipkart'
  URL = 'https://flipkart.com/search?q=Sunglasses'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.EMERGING_MARKET, story_tags.YEAR_2016]

  ITEM_SELECTOR = '[style="background-image: none;"]'
  BACK_SELECTOR = '._3NH1qf'
  ITEMS_TO_VISIT = 4
  IS_SINGLE_PAGE_APP = True

  def _WaitForNavigation(self, action_runner):
    action_runner.WaitForElement(text='Details')

  def _NavigateBack(self, action_runner):
    action_runner.ClickElement(selector=self.BACK_SELECTOR)
    action_runner.WaitForElement(text="Sunglasses")


class BrowseAmazonMobileStory(_ArticleBrowsingStory):
  NAME = 'browse:shopping:amazon'
  URL = 'https://www.amazon.co.in/s/?field-keywords=Mobile'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.EMERGING_MARKET, story_tags.YEAR_2016]

  ITEM_SELECTOR = '.aw-search-results'
  ITEMS_TO_VISIT = 4


class BrowseLazadaMobileStory(_ArticleBrowsingStory):
  NAME = 'browse:shopping:lazada'
  URL = 'https://www.lazada.co.id/catalog/?q=Wrist+watch'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.EMERGING_MARKET, story_tags.YEAR_2016]

  ITEM_SELECTOR = '.merchandise__link'
  ITEMS_TO_VISIT = 1


class BrowseAvitoMobileStory(_ArticleBrowsingStory):
  NAME = 'browse:shopping:avito'
  URL = 'https://www.avito.ru/rossiya'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.EMERGING_MARKET, story_tags.HEALTH_CHECK,
          story_tags.YEAR_2016]

  ITEM_SELECTOR = '.item-link'
  ITEMS_TO_VISIT = 4


class BrowseTOIMobileStory(_ArticleBrowsingStory):
  NAME = 'browse:news:toi'
  URL = 'http://m.timesofindia.com'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.EMERGING_MARKET, story_tags.YEAR_2016]

  ITEMS_TO_VISIT = 4
  ITEM_SELECTOR = '.dummy-img'


class BrowseGloboMobileStory(_ArticleBrowsingStory):
  NAME = 'browse:news:globo'
  URL = 'http://www.globo.com'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.EMERGING_MARKET, story_tags.YEAR_2016]

  ITEMS_TO_VISIT = 3  # 4 links causes renderer OOM crbug.com/714650.
  ITEM_SELECTOR = '.hui-premium__title'
  COMPLETE_STATE_WAIT_TIMEOUT = 150


class BrowseCricBuzzMobileStory(_ArticleBrowsingStory):
  NAME = 'browse:news:cricbuzz'
  URL = 'http://m.cricbuzz.com'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.EMERGING_MARKET, story_tags.YEAR_2016]

  ITEMS_TO_VISIT = 3
  ITEM_SELECTOR = '.list-content'



##############################################################################
# Maps browsing stories.
##############################################################################


class GoogleMapsMobileStory(system_health_story.SystemHealthStory):
  """Story that browses google maps mobile page

  This story searches for nearby restaurants on google maps website and finds
  directions to a chosen restaurant from search results.
  """
  NAME = 'browse:tools:maps'
  URL = 'https://maps.google.com/'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.EMERGING_MARKET, story_tags.YEAR_2016]

  _MAPS_SEARCH_BOX_SELECTOR = '.ml-searchbox-placeholder'
  _RESTAURANTS_LOADED = '.ml-panes-categorical-list-results'
  _SEARCH_NEW_AREA_SELECTOR = '.ml-reissue-search-button-inner'
  _RESTAURANTS_LINK = '.ml-entity-list-item-info'
  _DIRECTIONS_LINK = '[class="ml-button ml-inner-button-directions-fab"]'
  _DIRECTIONS_LOADED = ('[class="ml-fab-inner '
                        'ml-button ml-button-navigation-fab"]')
  _MAP_LAYER = '.ml-map'

  def _DidLoadDocument(self, action_runner):
    # Submit search query.
    self._ClickLink(self._MAPS_SEARCH_BOX_SELECTOR, action_runner)
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
    action_runner.EnterText('Google Mountain View')
    action_runner.PressKey('Return')
    action_runner.WaitForElement(selector=self._DIRECTIONS_LOADED)
    action_runner.WaitForNetworkQuiescence()
    action_runner.Wait(2) # Seeing direction

  def _ClickLink(self, selector, action_runner):
    action_runner.WaitForElement(selector=selector)
    action_runner.ClickElement(selector=selector)


class GoogleMapsStory(_BrowsingStory):
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
  NAME = 'browse:tools:maps'
  URL = 'https://www.maps.google.com/maps'
  _MAPS_SEARCH_BOX_SELECTOR = 'input[aria-label="Search Google Maps"]'
  _MAPS_ZOOM_IN_SELECTOR = '[aria-label="Zoom in"]'
  _RESTAURANTS_LOADED = ('[class="searchbox searchbox-shadow noprint '
                         'clear-button-shown"]')
  _RESTAURANTS_LINK = '[data-result-index="1"]'
  _DIRECTIONS_LINK = '[class="section-hero-header-directions-icon"]'
  _DIRECTIONS_FROM_BOX = '[class="tactile-searchbox-input"]'
  _DIRECTIONS_LOADED = '[class="section-directions-trip clearfix selected"]'
  # Get the current server response hash and store it for use
  # in _CHECK_RESTAURANTS_UPDATED.
  _GET_RESTAURANT_RESPONSE_HASH = '''
    document.querySelectorAll('[data-result-index="1"]')[0]
        .getAttribute('jstrack')
  '''
  # Check if the current restaurant serever response hash is different from
  # the old one to checks that restaurants started to update. Also wait for
  # the completion of the loading by waiting for the button to change to loaded.
  # The response hash gets updated when we scroll or zoom since server provides
  # a new response for the updated locations with a new hash value.
  _CHECK_RESTAURANTS_UPDATED = '''
    (document.querySelectorAll('[data-result-index="1"]')[0]
        .getAttribute('jstrack') != {{ old_restaurant }})
    && (document.querySelectorAll(
          '[class="searchbox searchbox-shadow noprint clear-button-shown"]')[0]
          != null)
    '''
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.JAVASCRIPT_HEAVY, story_tags.WEBGL,
          story_tags.YEAR_2016]

  def _DidLoadDocument(self, action_runner):
    # Click on the search box.
    action_runner.WaitForElement(selector=self._MAPS_SEARCH_BOX_SELECTOR)
    action_runner.WaitForElement(selector=self._MAPS_ZOOM_IN_SELECTOR)
    action_runner.ClickElement(selector=self._MAPS_SEARCH_BOX_SELECTOR)

    # Submit search query.
    action_runner.EnterText('restaurants near me')
    action_runner.PressKey('Return')
    action_runner.WaitForElement(selector=self._RESTAURANTS_LOADED)
    action_runner.Wait(1)

    # ZoomIn two times.
    prev_restaurant_hash = action_runner.EvaluateJavaScript(
        self._GET_RESTAURANT_RESPONSE_HASH)
    action_runner.ClickElement(selector=self._MAPS_ZOOM_IN_SELECTOR)
    action_runner.WaitForJavaScriptCondition(
        self._CHECK_RESTAURANTS_UPDATED,
        old_restaurant=prev_restaurant_hash, timeout=90)
    # This wait is required to fetch the data for all the tiles in the map.
    action_runner.Wait(1)

    prev_restaurant_hash = action_runner.EvaluateJavaScript(
        self._GET_RESTAURANT_RESPONSE_HASH)
    action_runner.ClickElement(selector=self._MAPS_ZOOM_IN_SELECTOR)
    action_runner.WaitForJavaScriptCondition(
        self._CHECK_RESTAURANTS_UPDATED,
        old_restaurant=prev_restaurant_hash, timeout=90)
    # This wait is required to fetch the data for all the tiles in the map.
    action_runner.Wait(1)

    # Reduce the speed (the current wpr is recorded with speed set to 50)  when
    # recording the wpr. If we scroll too fast, the data will not be recorded
    # well. After recording reset it back to the original value to have a more
    # realistic scroll.
    prev_restaurant_hash = action_runner.EvaluateJavaScript(
        self._GET_RESTAURANT_RESPONSE_HASH)
    action_runner.RepeatableBrowserDrivenScroll(
        x_scroll_distance_ratio = 0.0, y_scroll_distance_ratio = 0.5,
        repeat_count=2, speed=500, timeout=120, repeat_delay_ms=2000)
    action_runner.WaitForJavaScriptCondition(
        self._CHECK_RESTAURANTS_UPDATED,
        old_restaurant=prev_restaurant_hash, timeout=90)

    prev_restaurant_hash = action_runner.EvaluateJavaScript(
        self._GET_RESTAURANT_RESPONSE_HASH)
    action_runner.RepeatableBrowserDrivenScroll(
        x_scroll_distance_ratio = 0.5, y_scroll_distance_ratio = 0,
        repeat_count=2, speed=500, timeout=120, repeat_delay_ms=2000)
    action_runner.WaitForJavaScriptCondition(
        self._CHECK_RESTAURANTS_UPDATED,
        old_restaurant=prev_restaurant_hash, timeout=90)

    # To make the recording more realistic.
    action_runner.Wait(1)
    action_runner.ClickElement(selector=self._RESTAURANTS_LINK)
    # To make the recording more realistic.
    action_runner.Wait(1)
    action_runner.WaitForElement(selector=self._DIRECTIONS_LINK)
    action_runner.ClickElement(selector=self._DIRECTIONS_LINK)
    action_runner.ClickElement(selector=self._DIRECTIONS_FROM_BOX)
    action_runner.EnterText('6 Pancras Road London')
    action_runner.PressKey('Return')
    action_runner.WaitForElement(selector=self._DIRECTIONS_LOADED)
    action_runner.Wait(2)


class GoogleEarthStory(_BrowsingStory):
  """
  Google Earth story:
    _ Start at https://www.maps.google.com/maps
    _ Click on the Earth link
    _ Click ZoomIn three times, waiting for 3 sec in between.

  """
  # When recording this story:
  # Force tactile using this: http://google.com/maps?force=tt
  # Force webgl using this: http://google.com/maps?force=webgl
  # Change the speed as mentioned in the comment below for
  # RepeatableBrowserDrivenScroll
  NAME = 'browse:tools:earth'
  # Randomly picked location.
  URL = 'https://www.google.co.uk/maps/@51.4655936,-0.0985949,3329a,35y,40.58t/data=!3m1!1e3'
  _EARTH_BUTTON_SELECTOR = '[aria-labelledby="widget-minimap-caption"]'
  _EARTH_ZOOM_IN_SELECTOR = '[aria-label="Zoom in"]'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.JAVASCRIPT_HEAVY, story_tags.WEBGL,
          story_tags.YEAR_2016]

  def _DidLoadDocument(self, action_runner):
    # Zommin three times.
    action_runner.WaitForElement(selector=self._EARTH_ZOOM_IN_SELECTOR)
    action_runner.ClickElement(selector=self._EARTH_ZOOM_IN_SELECTOR)
    # To make the recording more realistic.
    action_runner.Wait(1)
    action_runner.ClickElement(selector=self._EARTH_ZOOM_IN_SELECTOR)
    # To make the recording more realistic.
    action_runner.Wait(1)
    action_runner.ClickElement(selector=self._EARTH_ZOOM_IN_SELECTOR)
    # To make the recording more realistic.
    action_runner.Wait(1)
    action_runner.ClickElement(selector=self._EARTH_ZOOM_IN_SELECTOR)
    action_runner.Wait(4)

    # Reduce the speed (the current wpr is recorded with speed set to 50)  when
    # recording the wpr. If we scroll too fast, the data will not be recorded
    # well. After recording reset it back to the original value to have a more
    # realistic scroll.
    action_runner.RepeatableBrowserDrivenScroll(
        x_scroll_distance_ratio = 0.0, y_scroll_distance_ratio = 1,
        repeat_count=3, speed=400, timeout=120)
    action_runner.RepeatableBrowserDrivenScroll(
        x_scroll_distance_ratio = 1, y_scroll_distance_ratio = 0,
        repeat_count=3, speed=500, timeout=120)


##############################################################################
# Browsing stories with infinite scrolling
##############################################################################


class _InfiniteScrollStory(system_health_story.SystemHealthStory):
  SUPPORTED_PLATFORMS = platforms.ALL_PLATFORMS

  SCROLL_DISTANCE = 25000
  SCROLL_STEP = 1000
  MAX_SCROLL_RETRIES = 3
  TIME_TO_WAIT_BEFORE_STARTING_IN_SECONDS = 5

  def __init__(self, story_set, take_memory_measurement):
    super(_InfiniteScrollStory, self).__init__(story_set,
        take_memory_measurement)
    self.script_to_evaluate_on_commit = '''
        window.WebSocket = undefined;
        window.Worker = undefined;
        window.performance = undefined;'''

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


class DiscourseDesktopStory(_InfiniteScrollStory):
  NAME = 'browse:tech:discourse_infinite_scroll'
  URL = ('https://meta.discourse.org/t/the-official-discourse-tags-plugin-discourse-tagging/26482')
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.INFINITE_SCROLL, story_tags.YEAR_2016]


class DiscourseDesktopStory2018(_InfiniteScrollStory):
  NAME = 'browse:tech:discourse_infinite_scroll:2018'
  URL = 'https://meta.discourse.org/t/topic-list-previews/41630/28'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.INFINITE_SCROLL, story_tags.YEAR_2018]


class DiscourseMobileStory(_InfiniteScrollStory):
  NAME = 'browse:tech:discourse_infinite_scroll'
  URL = ('https://meta.discourse.org/t/the-official-discourse-tags-plugin-discourse-tagging/26482')
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  SCROLL_DISTANCE = 15000
  TAGS = [story_tags.INFINITE_SCROLL, story_tags.YEAR_2016]


class DiscourseMobileStory2018(_InfiniteScrollStory):
  NAME = 'browse:tech:discourse_infinite_scroll:2018'
  URL = 'https://meta.discourse.org/t/topic-list-previews/41630/28'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.INFINITE_SCROLL, story_tags.YEAR_2018]


class FacebookScrollDesktopStory(_InfiniteScrollStory):
  NAME = 'browse:social:facebook_infinite_scroll'
  URL = 'https://www.facebook.com/shakira'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.INFINITE_SCROLL, story_tags.YEAR_2016]


class FacebookScrollDesktopStory2018(_InfiniteScrollStory):
  NAME = 'browse:social:facebook_infinite_scroll:2018'
  URL = 'https://www.facebook.com/shakira'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.INFINITE_SCROLL, story_tags.YEAR_2018]

  def _Login(self, action_runner):
    facebook_login.LoginWithDesktopSite(action_runner, 'facebook3')


class FacebookScrollMobileStory(_InfiniteScrollStory):
  NAME = 'browse:social:facebook_infinite_scroll'
  URL = 'https://m.facebook.com/shakira'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.YEAR_2016]

  def _Login(self, action_runner):
    facebook_login.LoginWithMobileSite(action_runner, 'facebook3')


class FacebookScrollMobileStory2018(_InfiniteScrollStory):
  NAME = 'browse:social:facebook_infinite_scroll:2018'
  URL = 'https://m.facebook.com/shakira'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.YEAR_2018]

  def _Login(self, action_runner):
    facebook_login.LoginWithMobileSite(action_runner, 'facebook3')

class FlickrDesktopStory(_InfiniteScrollStory):
  NAME = 'browse:media:flickr_infinite_scroll'
  URL = 'https://www.flickr.com/explore'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.INFINITE_SCROLL, story_tags.YEAR_2016]


class FlickrMobileStory(_InfiniteScrollStory):
  NAME = 'browse:media:flickr_infinite_scroll'
  URL = 'https://www.flickr.com/explore'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  SCROLL_DISTANCE = 10000
  TAGS = [story_tags.INFINITE_SCROLL, story_tags.YEAR_2016]


class PinterestMobileStory(_InfiniteScrollStory):
  NAME = 'browse:social:pinterest_infinite_scroll'
  URL = 'https://www.pinterest.com/all'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.INFINITE_SCROLL, story_tags.YEAR_2016]


class TumblrStory(_InfiniteScrollStory):
  NAME = 'browse:social:tumblr_infinite_scroll'
  URL = 'http://techcrunch.tumblr.com/'  # This page doesn't support HTTPS.
  TAGS = [story_tags.INFINITE_SCROLL, story_tags.JAVASCRIPT_HEAVY,
          story_tags.YEAR_2016]


class TwitterScrollDesktopStory(_InfiniteScrollStory):
  NAME = 'browse:social:twitter_infinite_scroll'
  URL = 'https://twitter.com/taylorswift13'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.INFINITE_SCROLL, story_tags.YEAR_2016]


class TwitterScrollDesktopStory2018(_InfiniteScrollStory):
  NAME = 'browse:social:twitter_infinite_scroll:2018'
  URL = 'https://twitter.com/NASA'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.INFINITE_SCROLL, story_tags.YEAR_2018]
