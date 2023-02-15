# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state
from telemetry.util import wpr_modes

from page_sets.login_helpers import google_login
from page_sets.login_helpers import linkedin_login
from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags


class TopRealWorldDesktopPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  TAGS = [story_tags.GPU_RASTERIZATION, story_tags.TOP_REAL_WORLD_DESKTOP]

  def __init__(self,
               page_set,
               shared_page_state_class,
               name_suffix='',
               extra_browser_args=None):
    super(TopRealWorldDesktopPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(1)
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage()
      if self.story_set.scroll_forever:
        while True:
          action_runner.ScrollPage(direction='up')
          action_runner.ScrollPage(direction='down')


class GoogleWebSearch2018Page(TopRealWorldDesktopPage):
  """ Why: top google property; a google tab is often open """
  BASE_NAME = 'google_web_search'
  YEAR = '2018'
  URL = 'https://www.google.com/#hl=en&q=barack+obama'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(GoogleWebSearch2018Page, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(GoogleWebSearch2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='Next')


class GoogleImageSearch2018Page(TopRealWorldDesktopPage):
  """ Why: tough image case; top google properties """
  BASE_NAME = 'google_image_search'
  YEAR = '2018'
  URL = 'https://www.google.com/search?q=cats&tbm=isch'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(GoogleImageSearch2018Page, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)


class GooglePlus2018Page(TopRealWorldDesktopPage):
  """ Why: social; top google property; Public profile; infinite scrolls """
  BASE_NAME = 'google_plus'
  YEAR = '2018'
  URL = 'https://plus.google.com/110031535020051778989/posts'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(GooglePlus2018Page, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(GooglePlus2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='Posts')


class Youtube2018Page(TopRealWorldDesktopPage):
  """ Why: #3 (Alexa global) """
  BASE_NAME = 'youtube'
  YEAR = '2018'
  URL = 'http://www.youtube.com'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(Youtube2018Page, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(Youtube2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(selector='#buttons')


class Blogspot2018Page(TopRealWorldDesktopPage):
  """ Why: #11 (Alexa global), google property; some blogger layouts have
  infinite scroll but more interesting """
  BASE_NAME = 'blogspot'
  YEAR = '2018'
  URL = 'http://googlewebmastercentral.blogspot.com/'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(Blogspot2018Page, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(Blogspot2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement('div[class="searchBox"]')


class Wordpress2018Page(TopRealWorldDesktopPage):
  """ Why: #18 (Alexa global), Picked an interesting post """
  BASE_NAME = 'wordpress'
  YEAR = '2018'
  # pylint: disable=line-too-long
  URL = 'http://en.blog.wordpress.com/2012/09/04/freshly-pressed-editors-picks-for-august-2012/'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(Wordpress2018Page, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(Wordpress2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(
        # pylint: disable=line-too-long
        'a[href="https://en.blog.wordpress.com/2012/08/30/new-themes-able-and-sight/"]'
    )


class Facebook2018Page(TopRealWorldDesktopPage):
  """ Why: top social,Public profile """
  BASE_NAME = 'facebook'
  YEAR = '2018'
  URL = 'https://www.facebook.com/barackobama'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(Facebook2018Page, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(Facebook2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='Videos')


class Linkedin2018Page(TopRealWorldDesktopPage):
  """ Why: #12 (Alexa global), Public profile. """
  BASE_NAME = 'linkedin'
  YEAR = '2018'
  URL = 'http://www.linkedin.com/in/linustorvalds'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(Linkedin2018Page, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    if self.wpr_mode != wpr_modes.WPR_REPLAY:
      linkedin_login.LoginDesktopAccount(action_runner, 'linkedin')
    super(Linkedin2018Page, self).RunNavigateSteps(action_runner)


class Wikipedia2018Page(TopRealWorldDesktopPage):
  """ Why: #6 (Alexa) most visited worldwide,Picked an interesting page. """
  BASE_NAME = 'wikipedia'
  YEAR = '2018'
  URL = 'http://en.wikipedia.org/wiki/Wikipedia'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(Wikipedia2018Page, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)


class Twitter2018Page(TopRealWorldDesktopPage):
  """ Why: #8 (Alexa global),Picked an interesting page """
  BASE_NAME = 'twitter'
  YEAR = '2018'
  URL = 'https://twitter.com/katyperry'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(Twitter2018Page, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(Twitter2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(selector='.ProfileNav')


class Pinterest2018Page(TopRealWorldDesktopPage):
  """ Why: #37 (Alexa global) """
  BASE_NAME = 'pinterest'
  YEAR = '2018'
  URL = 'https://www.pinterest.com/search/pins/?q=flowers&rs=typed'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(Pinterest2018Page, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)


class AccuWeather2018Page(TopRealWorldDesktopPage):
  """ Why: #2 weather according to Alexa """
  BASE_NAME = 'accu_weather'
  YEAR = '2018'
  URL = 'https://www.accuweather.com/en/us/new-york-ny/10017/weather-forecast/349727'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(AccuWeather2018Page, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(AccuWeather2018Page, self).RunNavigateSteps(action_runner)

    # Close a pop-up dialog before scrolling.
    action_runner.WaitForElement(selector=".fc-button-consent")
    action_runner.TapElement(selector=".fc-button-consent")


class Twitch2018Page(TopRealWorldDesktopPage):
  """ Why: #1 games according to Alexa  """
  BASE_NAME = 'twitch'
  YEAR = '2018'
  URL = 'https://www.twitch.tv'
  TAGS = TopRealWorldDesktopPage.TAGS + [
    story_tags.REPRESENTATIVE_MAC_DESKTOP
  ]

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(Twitch2018Page, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunPageInteractions(self, action_runner):
    action_runner.WaitForElement(selector='#mantle_skin')
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPageToElement(selector='.footer')
      if self.story_set.scroll_forever:
        while True:
          action_runner.ScrollPage(direction='up')
          action_runner.ScrollPage(direction='down')


class Gmail2018SmoothPage(TopRealWorldDesktopPage):
  """ Why: productivity, top google properties """
  BASE_NAME = 'gmail'
  YEAR = '2018'
  URL = 'https://mail.google.com/mail/'
  EXTRA_BROWSER_ARGUMENTS = ['--allow-browser-signin=false']

  def RunNavigateSteps(self, action_runner):
    if self.wpr_mode != wpr_modes.WPR_REPLAY:
      if self.wpr_mode in [wpr_modes.WPR_OFF, wpr_modes.WPR_RECORD]:
        google_login.LoginWithLoginUrl(action_runner, self.URL)
      else:
        google_login.NewLoginGoogleAccount(action_runner, 'googletest')

    super(Gmail2018SmoothPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'window.gmonkey !== undefined &&'
        'document.getElementById("gb") !== null &&'
        'document.readyState == "complete"')

  def RunPageInteractions(self, action_runner):
    action_runner.WaitForElement(selector='.Tm.aeJ')
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollElement(selector='.Tm.aeJ')
      if self.story_set.scroll_forever:
        while True:
          action_runner.ScrollElement(
              direction='up', selector='.Tm.aeJ')
          action_runner.ScrollElement(
              direction='down', selector='.Tm.aeJ')


class GoogleCalendar2018SmoothPage(TopRealWorldDesktopPage):
  """ Why: productivity, top google properties """
  BASE_NAME='google_calendar'
  YEAR = '2018'
  URL='https://www.google.com/calendar/'
  EXTRA_BROWSER_ARGUMENTS = ['--allow-browser-signin=false']

  def RunNavigateSteps(self, action_runner):
    if self.wpr_mode != wpr_modes.WPR_REPLAY:
      if self.wpr_mode in [wpr_modes.WPR_OFF, wpr_modes.WPR_RECORD]:
        google_login.LoginWithLoginUrl(action_runner, self.URL)
      else:
        google_login.NewLoginGoogleAccount(action_runner, 'googletest')
    super(GoogleCalendar2018SmoothPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement('span[class~="sm8sCf"]')
    action_runner.ExecuteJavaScript("""
        (function() {
          var elem = document.createElement('meta');
          elem.name='viewport';
          elem.content='initial-scale=1';
          document.body.appendChild(elem);
        })();""")
    action_runner.Wait(1)


  def RunPageInteractions(self, action_runner):
    action_runner.WaitForElement('span[class~="sm8sCf"]')
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollElement(selector='#YPCqFe')
      if self.story_set.scroll_forever:
        while True:
          action_runner.ScrollElement(
              direction='up', selector='#YPCqFe')
          action_runner.ScrollElement(
              direction='down', selector='#YPCqFe')


class GoogleDoc2018SmoothPage(TopRealWorldDesktopPage):
  """ Why: productivity, top google properties; Sample doc in the link """
  # pylint: disable=line-too-long
  URL = 'https://docs.google.com/document/d/1X-IKNjtEnx-WW5JIKRLsyhz5sbsat3mfTpAPUSX3_s4/view'
  BASE_NAME='google_docs'
  YEAR = '2018'

  def RunNavigateSteps(self, action_runner):
    super(GoogleDoc2018SmoothPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementsByClassName("kix-appview-editor").length')

  def RunPageInteractions(self, action_runner):
    action_runner.WaitForElement(selector='#printButton')
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollElement(selector='.kix-appview-editor')
      if self.story_set.scroll_forever:
        while True:
          action_runner.ScrollElement(
              direction='up', selector='.kix-appview-editor')
          action_runner.ScrollElement(
              direction='down', selector='.kix-appview-editor')


class ESPN2018SmoothPage(TopRealWorldDesktopPage):
  """ Why: #1 sports """
  BASE_NAME='espn'
  YEAR = '2018'
  URL = 'http://espn.go.com'

  def RunPageInteractions(self, action_runner):
    action_runner.WaitForElement(selector='#global-scoreboard')
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage(left_start_ratio=0.1)
      if self.story_set.scroll_forever:
        while True:
          action_runner.ScrollPage(direction='up', left_start_ratio=0.1)
          action_runner.ScrollPage(direction='down', left_start_ratio=0.1)


class YahooNews2018Page(TopRealWorldDesktopPage):
  """Why: #1 news worldwide (Alexa global)"""
  BASE_NAME = 'yahoo_news'
  YEAR = '2018'
  URL = 'http://news.yahoo.com'


class CNNNews2018Page(TopRealWorldDesktopPage):
  """Why: #2 news worldwide"""
  BASE_NAME = 'cnn'
  YEAR = '2018'
  URL = 'http://www.cnn.com'


class Amazon2018Page(TopRealWorldDesktopPage):
  # Why: #1 world commerce website by visits; #3 commerce in the US by
  # time spent
  BASE_NAME = 'amazon'
  YEAR = '2018'
  URL = 'http://www.amazon.com'


class Ebay2018Page(TopRealWorldDesktopPage):
  # Why: #1 commerce website by time spent by users in US
  BASE_NAME = 'ebay'
  YEAR = '2018'
  URL = 'http://www.ebay.com'


class Booking2018Page(TopRealWorldDesktopPage):
  # Why: #1 Alexa recreation
  BASE_NAME = 'booking.com'
  YEAR = '2018'
  URL = 'http://booking.com'


class YahooAnswers2018Page(TopRealWorldDesktopPage):
  # Why: #1 Alexa reference
  BASE_NAME = 'yahoo_answers'
  YEAR = '2018'
  URL = 'http://answers.yahoo.com'


class YahooSports2018Page(TopRealWorldDesktopPage):
  # Why: #1 Alexa sports
  BASE_NAME = 'yahoo_sports'
  YEAR = '2018'
  URL = 'http://sports.yahoo.com/'


class TechCrunch2018Page(TopRealWorldDesktopPage):
  # Why: top tech blog
  BASE_NAME = 'techcrunch'
  YEAR = '2018'
  URL = 'http://techcrunch.com'
