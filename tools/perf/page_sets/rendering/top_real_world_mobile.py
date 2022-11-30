# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state

from page_sets.login_helpers import linkedin_login
from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags
from page_sets.system_health import platforms


class TopRealWorldMobilePage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  TAGS = [story_tags.TOP_REAL_WORLD_MOBILE]
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(TopRealWorldMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage()


class CapitolVolkswagenMobile2018Page(TopRealWorldMobilePage):
  """ Why: Typical mobile business site """
  BASE_NAME = 'capitolvolkswagen_mobile'
  YEAR = '2018'
  URL = 'https://www.capitolvolkswagen.com/'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(CapitolVolkswagenMobile2018Page, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)


class TheVergeArticleMobile2018Page(TopRealWorldMobilePage):
  """ Why: Top tech blog """
  BASE_NAME = 'theverge_article_mobile'
  YEAR = '2018'
  # pylint: disable=line-too-long
  URL = 'https://www.theverge.com/2018/7/18/17582836/chrome-os-tablet-acer-chromebook-tab-10-android-ipad'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(TheVergeArticleMobile2018Page, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)


class CnnArticleMobile2018Page(TopRealWorldMobilePage):
  """ Why: Top news site """
  BASE_NAME = 'cnn_article_mobile'
  YEAR = '2018'
  # pylint: disable=line-too-long
  URL = 'https://www.cnn.com/travel/article/airbus-a330-900-neo-tours-us-airports/index.html'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(CnnArticleMobile2018Page, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(CnnArticleMobile2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(selector='.Article__entitlement')

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('ScrollAction'):
      # With default top_start_ratio=0.5 the corresponding element in this page
      # will not be in the root scroller.
      action_runner.ScrollPage(top_start_ratio=0.01)


class FacebookMobile2018Page(TopRealWorldMobilePage):
  """ Why: #1 (Alexa global) """
  BASE_NAME = 'facebook_mobile'
  YEAR = '2018'
  URL = 'https://facebook.com/barackobama'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(FacebookMobile2018Page, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(FacebookMobile2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("u_0_c") !== null &&'
        'document.body.scrollHeight > window.innerHeight')


class YoutubeMobile2018Page(TopRealWorldMobilePage):
  """ Why: #3 (Alexa global) """
  BASE_NAME = 'youtube_mobile'
  YEAR = '2018'
  URL = 'http://m.youtube.com/watch?v=9hBpF_Zj4OA'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(YoutubeMobile2018Page, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(YoutubeMobile2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("player") !== null')


class LinkedInMobile2018Page(TopRealWorldMobilePage):
  """ Why: #12 (Alexa global),Public profile """
  BASE_NAME = 'linkedin_mobile'
  YEAR = '2018'
  URL = 'https://www.linkedin.com/in/linustorvalds'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(LinkedInMobile2018Page, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  # Linkedin has expensive shader compilation so it can benefit from shader
  # cache from reload.
  def RunNavigateSteps(self, action_runner):
    linkedin_login.LoginMobileAccount(action_runner, 'linkedin')
    super(LinkedInMobile2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("profile-wrapper") !== null')

    action_runner.ScrollPage()

    super(LinkedInMobile2018Page, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("profile-wrapper") !== null')


class YahooAnswersMobile2018Page(TopRealWorldMobilePage):
  """ Why: #1 Alexa reference """
  BASE_NAME = 'yahoo_answers_mobile'
  YEAR = '2018'
  # pylint: disable=line-too-long
  URL = 'https://ca.answers.yahoo.com/'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(YahooAnswersMobile2018Page, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(YahooAnswersMobile2018Page, self).RunNavigateSteps(action_runner)
    action_runner.ScrollElement(selector='#page_scrollable')


class GoogleDocsMobile2022Page(TopRealWorldMobilePage):
  """ Why: productivity, top google properties; Sample doc in the link """
  # pylint: disable=line-too-long
  URL = 'https://docs.google.com/document/d/1X-IKNjtEnx-WW5JIKRLsyhz5sbsat3mfTpAPUSX3_s4/view'
  BASE_NAME = 'google_docs_mobile'
  YEAR = '2022'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(GoogleDocsMobile2022Page,
          self).__init__(page_set=page_set,
                         name_suffix=name_suffix,
                         extra_browser_args=extra_browser_args,
                         shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(GoogleDocsMobile2022Page, self).RunNavigateSteps(action_runner)
    # Wait for and close the pop-up window to make sure the entire doc is visible.
    action_runner.WaitForElement(selector='.docs-ml-promotion-no-button')
    action_runner.Wait(2)
    action_runner.TapElement(selector='.docs-ml-promotion-no-button')


class GoogleNewsMobile2018Page(TopRealWorldMobilePage):
  """ Why: Google News: accelerated scrolling version """
  BASE_NAME = 'google_news_mobile'
  YEAR = '2018'
  URL = 'https://news.google.com/'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(GoogleNewsMobile2018Page, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)


class GoogleImageSearchMobile2018Page(TopRealWorldMobilePage):
  """ Why: tough image case; top google properties """
  BASE_NAME = 'google_image_search_mobile'
  YEAR = '2018'
  URL = 'https://www.google.com/search?q=cats&tbm=isch'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(GoogleImageSearchMobile2018Page, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)


class AmazonNicolasCageMobile2018Page(TopRealWorldMobilePage):
  """
  Why: #1 world commerce website by visits; #3 commerce in the US by time spent
  """
  BASE_NAME = 'amazon_mobile'
  YEAR = '2018'
  URL = 'http://www.amazon.com/gp/aw/s/ref=is_box_?k=nicolas+cage'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(AmazonNicolasCageMobile2018Page, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)


class WowwikiMobile2018Page(TopRealWorldMobilePage):
  """Why: Mobile wiki."""
  BASE_NAME = 'wowwiki_mobile'
  YEAR = '2018'
  URL = 'http://www.wowwiki.com/World_of_Warcraft:_Mists_of_Pandaria'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(WowwikiMobile2018Page, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  # Wowwiki has expensive shader compilation so it can benefit from shader
  # cache from reload.
  def RunNavigateSteps(self, action_runner):
    super(WowwikiMobile2018Page, self).RunNavigateSteps(action_runner)
    action_runner.ScrollPage()
    super(WowwikiMobile2018Page, self).RunNavigateSteps(action_runner)


class WikipediaDelayedScrollMobile2018Page(TopRealWorldMobilePage):
  """Why: Wikipedia page with a delayed scroll start"""
  BASE_NAME = 'wikipedia_delayed_scroll_start'
  YEAR = '2018'
  URL = 'http://en.wikipedia.org/wiki/Wikipedia'

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(WikipediaDelayedScrollMobile2018Page, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunPageInteractions(self, action_runner):
    action_runner.WaitForJavaScriptCondition(
      'document.readyState == "complete"', timeout=30)
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage()


class BlogspotMobile2018Page(TopRealWorldMobilePage):
  """Why: #11 (Alexa global), google property"""
  BASE_NAME = 'blogspot_mobile'
  YEAR = '2018'
  URL = 'http://googlewebmastercentral.blogspot.com/'


class WordpressMobile2018Page(TopRealWorldMobilePage):
  """Why: #18 (Alexa global), Picked an interesting post"""
  BASE_NAME = 'wordpress_mobile'
  YEAR = '2018'
  # pylint: disable=line-too-long
  URL = 'http://en.blog.wordpress.com/2012/09/04/freshly-pressed-editors-picks-for-august-2012/'


class WikipediaMobile2018Page(TopRealWorldMobilePage):
  """Why: #6 (Alexa) most visited worldwide, picked an interesting page"""
  BASE_NAME = 'wikipedia_mobile'
  YEAR = '2018'
  URL = 'http://en.wikipedia.org/wiki/Wikipedia'


class TwitterMobile2018Page(TopRealWorldMobilePage):
  """Why: #8 (Alexa global), picked an interesting page"""
  BASE_NAME = 'twitter_mobile'
  YEAR = '2018'
  URL = 'http://twitter.com/katyperry'


class PinterestMobile2018Page(TopRealWorldMobilePage):
  """Why: #37 (Alexa global)."""
  BASE_NAME = 'pinterest_mobile'
  YEAR = '2018'
  URL = 'https://www.pinterest.com/search/pins/?q=flowers&rs=typed'


class ESPNMobile2018Page(TopRealWorldMobilePage):
  """Why: #1 sports."""
  BASE_NAME = 'espn_mobile'
  YEAR = '2018'
  URL = 'http://www.espn.com/'


class ForecastIOMobile2018Page(TopRealWorldMobilePage):
  """Why: crbug.com/231413"""
  BASE_NAME = 'forecast.io_mobile'
  YEAR = '2018'
  URL = 'http://forecast.io'


class GooglePlusMobile2018Page(TopRealWorldMobilePage):
  """Why: Social; top Google property; Public profile; infinite scrolls."""
  BASE_NAME = 'google_plus_mobile'
  YEAR = '2018'
  # pylint: disable=line-too-long
  URL = 'https://plus.google.com/app/basic/110031535020051778989/posts?source=apppromo'


class AndroidPoliceMobile2018Page(TopRealWorldMobilePage):
  """Why: crbug.com/242544"""
  BASE_NAME = 'androidpolice_mobile'
  YEAR = '2018'
  # pylint: disable=line-too-long
  URL = 'http://www.androidpolice.com/2012/10/03/rumor-evidence-mounts-that-an-lg-optimus-g-nexus-is-coming-along-with-a-nexus-phone-certification-program/'


class GSPMobile2018Page(TopRealWorldMobilePage):
  """Why: crbug.com/149958"""
  BASE_NAME = 'gsp.ro_mobile'
  YEAR = '2018'
  URL = 'http://gsp.ro'

  def RunNavigateSteps(self, action_runner):
    super(GSPMobile2018Page, self).RunNavigateSteps(action_runner)

    # Close a pop-up dialog that occludes a large area of the page.
    action_runner.WaitForElement(selector='.close-btn')
    action_runner.TapElement(selector='.close-btn')


class TheVergeMobile2018Page(TopRealWorldMobilePage):
  """Why: Top tech blog"""
  BASE_NAME = 'theverge_mobile'
  YEAR = '2018'
  URL = 'http://theverge.com'


class DiggMobile2018Page(TopRealWorldMobilePage):
  """Why: Top tech site"""
  BASE_NAME = 'digg_mobile'
  YEAR = '2018'
  URL = 'http://digg.com/channel/digg-feature'


class GoogleSearchMobile2018Page(TopRealWorldMobilePage):
  """Why: Top Google property; a Google tab is often open"""
  BASE_NAME = 'google_web_search_mobile'
  YEAR = '2018'
  URL = 'https://www.google.co.uk/search?hl=en&q=barack+obama&cad=h'


class YahooNewsMobile2018Page(TopRealWorldMobilePage):
  """Why: #1 news worldwide (Alexa global)"""
  BASE_NAME = 'yahoo_news_mobile'
  YEAR = '2018'
  URL = 'http://news.yahoo.com'


class CnnNewsMobile2018Page(TopRealWorldMobilePage):
  """# Why: #2 news worldwide"""
  BASE_NAME = 'cnn_mobile'
  YEAR = '2018'
  URL = 'http://www.cnn.com'


class EbayMobile2018Page(TopRealWorldMobilePage):
  """Why: #1 commerce website by time spent by users in US"""
  BASE_NAME = 'ebay_mobile'
  YEAR = '2018'
  URL = 'https://m.ebay.com/'


class BookingMobile2018Page(TopRealWorldMobilePage):
  """Why: #1 Alexa recreation"""
  BASE_NAME = 'booking.com_mobile'
  YEAR = '2018'
  # pylint: disable=line-too-long
  URL = 'https://www.booking.com'


class TechCrunchMobile2018Page(TopRealWorldMobilePage):
  """Why: Top tech blog"""
  BASE_NAME = 'techcrunch_mobile'
  YEAR = '2018'
  URL = 'http://techcrunch.com'


class MLBMobile2018Page(TopRealWorldMobilePage):
  """Why: #6 Alexa sports"""
  BASE_NAME = 'mlb_mobile'
  YEAR = '2018'
  URL = 'http://mlb.com/'


class SFGateMobile2018Page(TopRealWorldMobilePage):
  """Why: #14 Alexa California"""
  BASE_NAME = 'sfgate_mobile'
  YEAR = '2018'
  URL = 'http://www.sfgate.com/'


class WorldJournalMobile2018Page(TopRealWorldMobilePage):
  """Why: Non-latin character set"""
  BASE_NAME = 'worldjournal_mobile'
  YEAR = '2018'
  URL = 'http://worldjournal.com/'


class WSJMobile2018Page(TopRealWorldMobilePage):
  """Why: #15 Alexa news"""
  BASE_NAME = 'wsj_mobile'
  YEAR = '2018'
  URL = 'http://online.wsj.com/home-page'


class DeviantArtMobile2018Page(TopRealWorldMobilePage):
  """Why: Image-heavy mobile site"""
  BASE_NAME = 'deviantart_mobile'
  YEAR = '2018'
  URL = 'http://www.deviantart.com/'


class BaiduMobile2018Page(TopRealWorldMobilePage):
  """Why: Top search engine"""
  BASE_NAME = 'baidu_mobile'
  YEAR = '2018'
  # pylint: disable=line-too-long
  URL = 'http://www.baidu.com/s?wd=barack+obama&rsv_bp=0&rsv_spt=3&rsv_sug3=9&rsv_sug=0&rsv_sug4=3824&rsv_sug1=3&inputT=4920'


class BingMobile2018Page(TopRealWorldMobilePage):
  """Why: Top search engine"""
  BASE_NAME = 'bing_mobile'
  YEAR = '2018'
  URL = 'http://www.bing.com/search?q=sloths'


class USATodayMobile2018Page(TopRealWorldMobilePage):
  """Why: Good example of poor initial scrolling"""
  BASE_NAME = 'usatoday_mobile'
  YEAR = '2018'
  URL = 'http://ftw.usatoday.com/'


class FastPathSmoothMobilePage(TopRealWorldMobilePage):
  ABSTRACT_STORY = True
  TAGS = [story_tags.FASTPATH, story_tags.TOP_REAL_WORLD_MOBILE]

  def __init__(self,
               page_set,
               name_suffix='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(FastPathSmoothMobilePage, self).__init__(
        page_set=page_set,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)


class NYTimesMobile2018Page(FastPathSmoothMobilePage):
  """Why: Top news site."""
  BASE_NAME = 'nytimes_mobile'
  YEAR = '2018'
  URL = 'http://nytimes.com/'


class RedditMobile2018Page(FastPathSmoothMobilePage):
  """Why: #5 Alexa news."""
  BASE_NAME = 'reddit_mobile'
  YEAR = '2018'
  URL = 'http://www.reddit.com/r/programming/comments/1g96ve'


class BoingBoingMobile2018Page(FastPathSmoothMobilePage):
  """Why: Problematic use of fixed position elements."""
  BASE_NAME = 'boingboing_mobile'
  YEAR = '2018'
  URL = 'http://www.boingboing.net'


class SlashDotMobile2018Page(FastPathSmoothMobilePage):
  """Why: crbug.com/169827"""
  BASE_NAME = 'slashdot_mobile'
  YEAR = '2018'
  URL = 'http://slashdot.org'
