# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags
from page_sets.system_health import platforms
from page_sets.login_helpers import linkedin_login


class ToughPinchZoomMobilePage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.TOUGH_PINCH_ZOOM_MOBILE]

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(ToughPinchZoomMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

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
      current_scale_factor = 7.0
      self.RunPinchGesture(action_runner, scale_factor=current_scale_factor)
      while current_scale_factor > 1.0:
        current_scale_factor *= 1/2.0
        self.RunPinchGesture(action_runner, scale_factor=1/2.0)


class GoogleSearchPinchZoomMobile2018Page(ToughPinchZoomMobilePage):
  """ Why: top google property; a google tab is often open. """
  BASE_NAME = 'google_search_mobile_pinch'
  YEAR = '2018'
  URL = 'https://www.google.com/#hl=en&q=barack+obama'


class LinkedinPinchZoomMobile2018Page(ToughPinchZoomMobilePage):
  """ Why: #12 (Alexa global), Public profile."""
  BASE_NAME = 'linkedin_mobile_pinch'
  YEAR = '2018'
  URL = 'http://www.linkedin.com/in/linustorvalds'

  # Linkedin has expensive shader compilation so it can benefit from shader
  # cache from reload.
  def RunNavigateSteps(self, action_runner):
    linkedin_login.LoginMobileAccount(action_runner, 'linkedin')
    super(LinkedinPinchZoomMobile2018Page,
        self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("profile-wrapper") !== null')


class AccuWeatherPinchZoomMobile2018Page(ToughPinchZoomMobilePage):
  """ Why: #2 weather according to Alexa."""
  BASE_NAME = 'accu_weather_mobile_pinch'
  YEAR = '2018'
  URL = 'https://www.accuweather.com/en/us/new-york-ny/10017/weather-forecast/349727'


class TwitchPinchZoomMobile2018Page(ToughPinchZoomMobilePage):
  """ Why: #1 games according to Alexa."""
  BASE_NAME = 'twitch_mobile_pinch'
  YEAR = '2018'
  URL = 'https://www.twitch.tv/?no-mobile-redirect=true'


class CnnPinchZoomMobile2018Page(ToughPinchZoomMobilePage):
  """ Why: #2 news worldwide."""
  BASE_NAME = 'cnn_mobile_pinch'
  YEAR = '2018'
  URL = 'http://www.cnn.com/travel/article/airbus-a330-900-neo-tours-us-airports/index.html'


class EBayPinchZoomMobile2018Page(ToughPinchZoomMobilePage):
  """  Why: #1 commerce website by time spent by users in US."""
  BASE_NAME = 'ebay_mobile_pinch'
  YEAR = '2018'
  URL = 'http://www.ebay.com'
