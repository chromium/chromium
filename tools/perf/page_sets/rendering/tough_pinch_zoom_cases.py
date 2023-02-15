# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.util import wpr_modes

from page_sets.rendering import rendering_shared_state
from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags
from page_sets.system_health import platforms
from page_sets.login_helpers import linkedin_login
from page_sets.login_helpers import google_login


class ToughPinchZoomPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.GPU_RASTERIZATION, story_tags.TOUGH_PINCH_ZOOM]

  def __init__(self,
               page_set,
               name_suffix='',
               shared_page_state_class=(
                   rendering_shared_state.DesktopRenderingSharedState),
               extra_browser_args=None):
    super(ToughPinchZoomPage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

    self.target_scale_factor = page_set.target_scale_factor

  def RunPinchGesture(self, action_runner, left_anchor_ratio=0.5,
                      top_anchor_ratio=0.5, scale_factor=None,
                      speed_in_pixels_per_second=800):
    with action_runner.CreateGestureInteraction('PinchAction', repeatable=True):
      action_runner.PinchPage(
          left_anchor_ratio=left_anchor_ratio,
          top_anchor_ratio=top_anchor_ratio,
          scale_factor=scale_factor,
          speed_in_pixels_per_second=speed_in_pixels_per_second)

  def RunPageInteractions(self, action_runner):
    action_runner.tab.WaitForDocumentReadyStateToBeInteractiveOrBetter()
    for _ in range(3):
      current_scale_factor = self.target_scale_factor
      self.RunPinchGesture(action_runner, scale_factor=current_scale_factor)
      while current_scale_factor > 1.0:
        current_scale_factor *= 1/2.0
        self.RunPinchGesture(action_runner, scale_factor=1/2.0)


class GoogleSearchPinchZoom2018Page(ToughPinchZoomPage):

  """ Why: top google property; a google tab is often open. """

  BASE_NAME = 'google_search_pinch'
  YEAR = '2018'
  URL = 'https://www.google.com/#hl=en&q=barack+obama'

  def RunNavigateSteps(self, action_runner):
    super(GoogleSearchPinchZoom2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='Next')


class GmailPinchZoom2018Page(ToughPinchZoomPage):

  """ Why: productivity, top google properties """

  BASE_NAME = 'gmail_pinch'
  YEAR = '2018'
  URL = 'https://mail.google.com/mail/'
  EXTRA_BROWSER_ARGUMENTS = ['--allow-browser-signin=false']

  def RunNavigateSteps(self, action_runner):
    if self.wpr_mode != wpr_modes.WPR_REPLAY:
      if self.wpr_mode in [wpr_modes.WPR_OFF, wpr_modes.WPR_RECORD]:
        google_login.LoginWithLoginUrl(action_runner, self.URL)
      else:
        google_login.NewLoginGoogleAccount(action_runner, 'googletest')
    super(GmailPinchZoom2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'window.gmonkey !== undefined &&'
        'document.getElementById("gb") !== null')


class GoogleCalendarPinchZoom2018Page(ToughPinchZoomPage):

  """ Why: productivity, top google properties """

  BASE_NAME = 'google_calendar_pinch'
  YEAR = '2018'
  URL = 'https://www.google.com/calendar/'
  EXTRA_BROWSER_ARGUMENTS = ['--allow-browser-signin=false']

  def RunNavigateSteps(self, action_runner):
    if self.wpr_mode != wpr_modes.WPR_REPLAY:
      if self.wpr_mode in [wpr_modes.WPR_OFF, wpr_modes.WPR_RECORD]:
        google_login.LoginWithLoginUrl(action_runner, self.URL)
      else:
        google_login.NewLoginGoogleAccount(action_runner, 'googletest')
    super(GoogleCalendarPinchZoom2018Page, self).RunNavigateSteps(
      action_runner)
    action_runner.WaitForElement('span[class~="sm8sCf"]')


class GoogleImagePinchZoom2018Page(ToughPinchZoomPage):

  """ Why: tough image case; top google properties """

  BASE_NAME = 'google_image_pinch'
  YEAR = '2018'
  URL = 'https://www.google.com/search?q=cats&tbm=isch'


class YoutubePinchZoom2018Page(ToughPinchZoomPage):

  """ Why: #3 (Alexa global) """

  BASE_NAME = 'youtube_pinch'
  YEAR = '2018'
  URL = 'http://www.youtube.com'

  def RunNavigateSteps(self, action_runner):
    super(YoutubePinchZoom2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(selector='#buttons')


class BlogSpotPinchZoom2018Page(ToughPinchZoomPage):

  """
  Why: #11 (Alexa global), google property; some blogger layouts have infinite
  scroll but more interesting
  """

  BASE_NAME = 'blogspot_pinch'
  YEAR = '2018'
  URL = 'http://googlewebmastercentral.blogspot.com/'

  def RunNavigateSteps(self, action_runner):
    super(BlogSpotPinchZoom2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement('div[class="searchBox"]')


class FacebookPinchZoom2018Page(ToughPinchZoomPage):

  """ Why: top social,Public profile """

  BASE_NAME = 'facebook_pinch'
  YEAR = '2018'
  URL = 'http://www.facebook.com/barackobama'

  def RunNavigateSteps(self, action_runner):
    super(FacebookPinchZoom2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='Videos')


class LinkedinPinchZoom2018Page(ToughPinchZoomPage):

  """ Why: #12 (Alexa global),Public profile """

  BASE_NAME = 'linkedin_pinch'
  YEAR = '2018'
  URL = 'http://www.linkedin.com/in/linustorvalds'

  def RunNavigateSteps(self, action_runner):
    if self.wpr_mode != wpr_modes.WPR_REPLAY:
      linkedin_login.LoginDesktopAccount(action_runner, 'linkedin')
    super(LinkedinPinchZoom2018Page, self).RunNavigateSteps(action_runner)


class TwitterPinchZoom2018Page(ToughPinchZoomPage):

  """ Why: #8 (Alexa global),Picked an interesting page """

  BASE_NAME = 'twitter_pinch'
  YEAR = '2018'
  URL = 'https://twitter.com/katyperry'


  def RunNavigateSteps(self, action_runner):
    super(TwitterPinchZoom2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(selector='.ProfileNav')


class ESPNPinchZoom2018Page(ToughPinchZoomPage):

  """ Why: #1 sports """

  BASE_NAME = 'espn_pinch'
  YEAR = '2018'
  URL = 'http://espn.go.com/nba'


class AccuWeatherPinchZoom2018Page(ToughPinchZoomPage):
  """ Why: #2 weather according to Alexa """
  BASE_NAME = 'accu_weather_pinch'
  YEAR = '2018'
  URL = 'https://www.accuweather.com/en/us/new-york-ny/10017/weather-forecast/349727'


class TwitchPinchZoom2018Page(ToughPinchZoomPage):
  """ Why: #1 games according to Alexa  """
  BASE_NAME = 'twitch_pinch'
  YEAR = '2018'
  URL = 'https://www.twitch.tv'


class YahooNewsPinchZoom2018Page(ToughPinchZoomPage):

  """ Why: #1 news worldwide (Alexa global) """

  BASE_NAME = 'yahoo_news_pinch'
  YEAR = '2018'
  URL = 'http://news.yahoo.com'


class CnnPinchZoom2018Page(ToughPinchZoomPage):

  """ Why: #2 news worldwide """

  BASE_NAME = 'cnn_pinch'
  YEAR = '2018'
  URL = 'http://www.cnn.com'


class AmazonPinchZoom2018Page(ToughPinchZoomPage):

  """
  Why: #1 world commerce website by visits; #3 commerce in the US by
  time spent
  """

  BASE_NAME = 'amazon_pinch'
  YEAR = '2018'
  URL = 'http://www.amazon.com'


class EBayPinchZoom2018Page(ToughPinchZoomPage):

  """  Why: #1 commerce website by time spent by users in US"""

  BASE_NAME = 'ebay_pinch'
  YEAR = '2018'
  URL = 'http://www.ebay.com'


class BookingPinchZoom2018Page(ToughPinchZoomPage):

  """ Why: #1 Alexa recreation"""

  BASE_NAME = 'booking_pinch'
  YEAR = '2018'
  URL = 'http://booking.com'


class YahooSportsPinchZoom2018Page(ToughPinchZoomPage):

  """ Why: #1 Alexa sports"""
  BASE_NAME = 'yahoo_sports_pinch'
  YEAR = '2018'
  URL = 'http://sports.yahoo.com/'
