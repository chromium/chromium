# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state
from telemetry import story

from page_sets import top_pages


def _IssueMarkerAndScroll(action_runner, scroll_forever):
  with action_runner.CreateGestureInteraction('ScrollAction'):
    action_runner.ScrollPage()
    if scroll_forever:
      while True:
        action_runner.ScrollPage(direction='up')
        action_runner.ScrollPage(direction='down')


def _CreatePageClassWithSmoothInteractions(page_cls):

  class DerivedSmoothPage(page_cls):  # pylint: disable=no-init

    def RunPageInteractions(self, action_runner):
      action_runner.Wait(1)
      _IssueMarkerAndScroll(action_runner, self.story_set.scroll_forever)

  return DerivedSmoothPage

class Top25SmoothPage(top_pages.TopPages):

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(1)
    _IssueMarkerAndScroll(action_runner, self.story_set.scroll_forever)

class GmailSmoothPage(top_pages.GmailPage):
  """ Why: productivity, top google properties """

  def RunPageInteractions(self, action_runner):
    action_runner.ExecuteJavaScript("""
        gmonkey.load('2.0', function(api) {
          window.__scrollableElementForTelemetry = api.getScrollableElement();
        });""")
    action_runner.WaitForJavaScriptCondition(
        'window.__scrollableElementForTelemetry != null')
    action_runner.Wait(1)
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollElement(
          element_function='window.__scrollableElementForTelemetry')
      if self.story_set.scroll_forever:
        while True:
          action_runner.ScrollElement(
              direction='up',
              element_function='window.__scrollableElementForTelemetry')
          action_runner.ScrollElement(
              direction='down',
              element_function='window.__scrollableElementForTelemetry')


class GoogleCalendarSmoothPage(top_pages.GoogleCalendarPage):
  """ Why: productivity, top google properties """

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(1)
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollElement(selector='#scrolltimedeventswk')
      if self.story_set.scroll_forever:
        while True:
          action_runner.ScrollElement(
              direction='up', selector='#scrolltimedeventswk')
          action_runner.ScrollElement(
              direction='down', selector='#scrolltimedeventswk')


class GoogleDocSmoothPage(top_pages.GoogleDocPage):
  """ Why: productivity, top google properties; Sample doc in the link """

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(1)
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollElement(selector='.kix-appview-editor')
      if self.story_set.scroll_forever:
        while True:
          action_runner.ScrollElement(
              direction='up', selector='.kix-appview-editor')
          action_runner.ScrollElement(
              direction='down', selector='.kix-appview-editor')


class ESPNSmoothPage(top_pages.ESPNPage):
  """ Why: #1 sports """

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(1)
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage(left_start_ratio=0.1)
      if self.story_set.scroll_forever:
        while True:
          action_runner.ScrollPage(direction='up', left_start_ratio=0.1)
          action_runner.ScrollPage(direction='down', left_start_ratio=0.1)


_SMOOTH_PAGE_CLASSES = [
  (GmailSmoothPage, 'gmail'),
  (GoogleCalendarSmoothPage, 'google_calendar'),
  (GoogleDocSmoothPage, 'google_docs'),
  (ESPNSmoothPage, 'espn'),
]


_NON_SMOOTH_PAGE_CLASSES = [
  (top_pages.GoogleWebSearchPage, 'google_web_search'),
  (top_pages.GoogleImageSearchPage, 'google_image_search'),
  (top_pages.GooglePlusPage, 'google_plus'),
  (top_pages.YoutubePage, 'youtube'),
  (top_pages.BlogspotPage, 'blogspot'),
  (top_pages.WordpressPage, 'wordpress'),
  (top_pages.FacebookPage, 'facebook'),
  (top_pages.LinkedinPage, 'linkedin'),
  (top_pages.WikipediaPage, 'wikipedia'),
  (top_pages.TwitterPage, 'twitter'),
  (top_pages.PinterestPage, 'pinterest'),
  (top_pages.WeatherPage, 'weather.com'),
  (top_pages.YahooGamesPage, 'yahoo_games'),
]


_PAGE_URLS = [
  # Why: #1 news worldwide (Alexa global)
  ('http://news.yahoo.com', 'yahoo_news'),
  # Why: #2 news worldwide
  ('http://www.cnn.com', 'cnn'),
  # Why: #1 world commerce website by visits; #3 commerce in the US by
  # time spent
  ('http://www.amazon.com', 'amazon'),
  # Why: #1 commerce website by time spent by users in US
  ('http://www.ebay.com', 'ebay'),
  # Why: #1 Alexa recreation
  ('http://booking.com', 'booking.com'),
  # Why: #1 Alexa reference
  ('http://answers.yahoo.com', 'yahoo_answers'),
  # Why: #1 Alexa sports
  ('http://sports.yahoo.com/', 'yahoo_sports'),
  # Why: top tech blog
  ('http://techcrunch.com', 'techcrunch'),
]


def AddPagesToPageSet(
    page_set,
    shared_page_state_class=shared_page_state.SharedDesktopPageState,
    name_func=lambda name: name,
    extra_browser_args=None):
  for page_class, page_name in _SMOOTH_PAGE_CLASSES:
    page_set.AddStory(
        page_class(
            page_set=page_set,
            shared_page_state_class=shared_page_state_class,
            name=name_func(page_name),
            extra_browser_args=extra_browser_args))

  for page_class, page_name in _NON_SMOOTH_PAGE_CLASSES:
    page_set.AddStory(
        _CreatePageClassWithSmoothInteractions(page_class)(
            page_set=page_set,
            shared_page_state_class=shared_page_state_class,
            name=name_func(page_name),
            extra_browser_args=extra_browser_args))

  for page_url, page_name in _PAGE_URLS:
    page_set.AddStory(
        Top25SmoothPage(
            url=page_url,
            page_set=page_set,
            shared_page_state_class=shared_page_state_class,
            name=name_func(page_name),
            extra_browser_args=extra_browser_args))


class Top25SmoothPageSet(story.StorySet):
  """ Pages hand-picked for 2012 CrOS scrolling tuning efforts. """

  def __init__(self, scroll_forever=False):
    super(Top25SmoothPageSet, self).__init__(
        archive_data_file='data/top_25_smooth.json',
        cloud_storage_bucket=story.PARTNER_BUCKET)

    self.scroll_forever = scroll_forever

    AddPagesToPageSet(self)
