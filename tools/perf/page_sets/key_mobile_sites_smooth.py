# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import shared_page_state
from telemetry import story


FASTPATH_URLS = [
    # Why: Top news site.
    ('http://nytimes.com/', 'nytimes'),
    # Why: Image-heavy site.
    ('http://cuteoverload.com', 'cuteoverload'),
    # Why: #5 Alexa news.
    ('http://www.reddit.com/r/programming/comments/1g96ve', 'reddit'),
    # Why: Problematic use of fixed position elements.
    ('http://www.boingboing.net', 'boingboing'),
    # Why: crbug.com/169827
    ('http://slashdot.org', 'slashdot'),
]


URLS_LIST = [
    # Why: #11 (Alexa global), google property; some blogger layouts
    # have infinite scroll but more interesting.
    ('http://googlewebmastercentral.blogspot.com/', 'blogspot'),
    # Why: #18 (Alexa global), Picked an interesting post
    ('http://en.blog.wordpress.com/2012/09/04/freshly-pressed-editors-picks-for-august-2012/',
     'wordpress'),
    # Why: #6 (Alexa) most visited worldwide, picked an interesting page
    ('http://en.wikipedia.org/wiki/Wikipedia', 'wikipedia'),
    # Why: #8 (Alexa global), picked an interesting page
    ('http://twitter.com/katyperry', 'twitter'),
    # Why: #37 (Alexa global).
    ('http://pinterest.com', 'pinterest'),
    # Why: #1 sports.
    ('http://espn.go.com', 'espn'),
    # Why: crbug.com/231413
    ('http://forecast.io', 'forecast.io'),
    # Why: Social; top Google property; Public profile; infinite scrolls.
    ('https://plus.google.com/app/basic/110031535020051778989/posts?source=apppromo',
     'google_plus'),
    # Why: crbug.com/242544
    # pylint: disable=line-too-long
    ('http://www.androidpolice.com/2012/10/03/rumor-evidence-mounts-that-an-lg-optimus-g-nexus-is-coming-along-with-a-nexus-phone-certification-program/',
     'androidpolice'),
    # Why: crbug.com/149958
    ('http://gsp.ro', 'gsp.ro'),
    # Why: Top tech blog
    ('http://theverge.com', 'theverge'),
    # Why: Top tech site
    ('http://digg.com', 'digg'),
    # Why: Top Google property; a Google tab is often open
    ('https://www.google.co.uk/search?hl=en&q=barack+obama&cad=h',
     'google_web_search'),
    # Why: #1 news worldwide (Alexa global)
    ('http://news.yahoo.com', 'yahoo_news'),
    # Why: #2 news worldwide
    ('http://www.cnn.com', 'cnn'),
    # Why: #1 commerce website by time spent by users in US
    ('http://shop.mobileweb.ebay.com/searchresults?kw=viking+helmet', 'ebay'),
    # Why: #1 Alexa recreation
    # pylint: disable=line-too-long
    ('http://www.booking.com/searchresults.html?src=searchresults&latitude=65.0500&longitude=25.4667',
     'booking.com'),
    # Why: Top tech blog
    ('http://techcrunch.com', 'techcrunch'),
    # Why: #6 Alexa sports
    ('http://mlb.com/', 'mlb'),
    # Why: #14 Alexa California
    ('http://www.sfgate.com/', 'sfgate'),
    # Why: Non-latin character set
    ('http://worldjournal.com/', 'worldjournal'),
    # Why: #15 Alexa news
    ('http://online.wsj.com/home-page', 'wsj'),
    # Why: Image-heavy mobile site
    ('http://www.deviantart.com/', 'deviantart'),
    # Why: Top search engine
    # pylint: disable=line-too-long
    ('http://www.baidu.com/s?wd=barack+obama&rsv_bp=0&rsv_spt=3&rsv_sug3=9&rsv_sug=0&rsv_sug4=3824&rsv_sug1=3&inputT=4920',
     'baidu'),
    # Why: Top search engine
    ('http://www.bing.com/search?q=sloths', 'bing'),
    # Why: Good example of poor initial scrolling
    ('http://ftw.usatoday.com/2014/05/spelling-bee-rules-shenanigans',
     'usatoday'),
]


def _IssueMarkerAndScroll(action_runner):
  with action_runner.CreateGestureInteraction('ScrollAction'):
    action_runner.ScrollPage()


class KeyMobileSitesSmoothPage(page_module.Page):

  def __init__(self,
               url,
               page_set,
               name='',
               tags=None,
               action_on_load_complete=False,
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    if name == '':
      name = url
    super(KeyMobileSitesSmoothPage, self).__init__(
        url=url,
        page_set=page_set,
        name=name,
        tags=tags,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)
    self.action_on_load_complete = action_on_load_complete

  def RunPageInteractions(self, action_runner):
    if self.action_on_load_complete:
      action_runner.WaitForJavaScriptCondition(
          'document.readyState == "complete"', timeout=30)
    _IssueMarkerAndScroll(action_runner)


class CapitolVolkswagenPage(KeyMobileSitesSmoothPage):
  """ Why: Typical mobile business site """

  def __init__(self,
               page_set,
               name='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(CapitolVolkswagenPage, self).__init__(
        url=(
            'http://iphone.capitolvolkswagen.com/index.htm'
            '#new-inventory_p_2Fsb-new_p_2Ehtm_p_3Freset_p_3DInventoryListing'),
        page_set=page_set,
        name=name,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(CapitolVolkswagenPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='Next 35')
    action_runner.WaitForJavaScriptCondition(
        'document.body.scrollHeight > 2560')


class TheVergeArticlePage(KeyMobileSitesSmoothPage):
  """ Why: Top tech blog """

  def __init__(self,
               page_set,
               name='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(TheVergeArticlePage, self).__init__(
        # pylint: disable=line-too-long
        url=
        'http://www.theverge.com/2012/10/28/3568746/amazon-7-inch-fire-hd-ipad-mini-ad-ballsy',
        page_set=page_set,
        name=name,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(TheVergeArticlePage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'window.Chorus !== undefined &&'
        'window.Chorus.Comments !== undefined &&'
        'window.Chorus.Comments.Json !== undefined &&'
        '(window.Chorus.Comments.loaded ||'
        ' window.Chorus.Comments.Json.load_comments())')


class CnnArticlePage(KeyMobileSitesSmoothPage):
  """ Why: Top news site """

  def __init__(self,
               page_set,
               name='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(CnnArticlePage, self).__init__(
        # pylint: disable=line-too-long
        url=
        'http://www.cnn.com/2012/10/03/politics/michelle-obama-debate/index.html',
        page_set=page_set,
        name=name,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(CnnArticlePage, self).RunNavigateSteps(action_runner)
    action_runner.Wait(8)

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('ScrollAction'):
      # With default top_start_ratio=0.5 the corresponding element in this page
      # will not be in the root scroller.
      action_runner.ScrollPage(top_start_ratio=0.01)


class FacebookPage(KeyMobileSitesSmoothPage):
  """ Why: #1 (Alexa global) """

  def __init__(self,
               page_set,
               name='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(FacebookPage, self).__init__(
        url='https://facebook.com/barackobama',
        page_set=page_set,
        name=name,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(FacebookPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("u_0_c") !== null &&'
        'document.body.scrollHeight > window.innerHeight')


class YoutubeMobilePage(KeyMobileSitesSmoothPage):
  """ Why: #3 (Alexa global) """

  def __init__(self,
               page_set,
               name='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(YoutubeMobilePage, self).__init__(
        url='http://m.youtube.com/watch?v=9hBpF_Zj4OA',
        page_set=page_set,
        name=name,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(YoutubeMobilePage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("paginatortarget") !== null')


class LinkedInPage(KeyMobileSitesSmoothPage):
  """ Why: #12 (Alexa global),Public profile """

  def __init__(self,
               page_set,
               name='LinkedIn',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(LinkedInPage, self).__init__(
        url='https://www.linkedin.com/in/linustorvalds',
        page_set=page_set,
        name=name,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  # Linkedin has expensive shader compilation so it can benefit from shader
  # cache from reload.
  def RunNavigateSteps(self, action_runner):
    super(LinkedInPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("profile-view-scroller") !== null')

    action_runner.ScrollPage()

    super(LinkedInPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("profile-view-scroller") !== null')


class YahooAnswersPage(KeyMobileSitesSmoothPage):
  """ Why: #1 Alexa reference """

  def __init__(self,
               page_set,
               name='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(YahooAnswersPage, self).__init__(
        # pylint: disable=line-too-long
        url='http://answers.yahoo.com/question/index?qid=20110117024343AAopj8f',
        page_set=page_set,
        name=name,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(YahooAnswersPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForElement(text='Other Answers (1 - 20 of 149)')
    action_runner.ClickElement(text='Other Answers (1 - 20 of 149)')


class GoogleNewsMobilePage(KeyMobileSitesSmoothPage):
  """ Why: Google News: accelerated scrolling version """

  def __init__(self,
               page_set,
               name='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(GoogleNewsMobilePage, self).__init__(
        url='http://mobile-news.sandbox.google.com/news/pt1',
        page_set=page_set,
        name=name,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunNavigateSteps(self, action_runner):
    super(GoogleNewsMobilePage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'typeof NEWS_telemetryReady !== "undefined" && '
        'NEWS_telemetryReady == true')


class AmazonNicolasCagePage(KeyMobileSitesSmoothPage):
  """
  Why: #1 world commerce website by visits; #3 commerce in the US by time spent
  """

  def __init__(self,
               page_set,
               name='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(AmazonNicolasCagePage, self).__init__(
        url='http://www.amazon.com/gp/aw/s/ref=is_box_?k=nicolas+cage',
        page_set=page_set,
        name=name,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollElement(
          selector='#search',
          distance_expr='document.body.scrollHeight - window.innerHeight')


class WowwikiPage(KeyMobileSitesSmoothPage):
  """Why: Mobile wiki."""

  def __init__(self,
               page_set,
               name='',
               extra_browser_args=None,
               shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(WowwikiPage, self).__init__(
        url='http://www.wowwiki.com/World_of_Warcraft:_Mists_of_Pandaria',
        page_set=page_set,
        name=name,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  # Wowwiki has expensive shader compilation so it can benefit from shader
  # cache from reload.
  def RunNavigateSteps(self, action_runner):
    super(WowwikiPage, self).RunNavigateSteps(action_runner)
    action_runner.ScrollPage()
    super(WowwikiPage, self).RunNavigateSteps(action_runner)


PREDEFINED_PAGE_CLASSES = [
    (CapitolVolkswagenPage, 'capitolvolkswagen'),
    (TheVergeArticlePage, 'theverge_article'),
    (FacebookPage, 'facebook'),
    (YoutubeMobilePage, 'youtube'),
    (YahooAnswersPage, 'yahoo_answers'),
    (GoogleNewsMobilePage, 'google_news'),
    (LinkedInPage, 'linkedin'),
    (WowwikiPage, 'wowwiki'),
    (AmazonNicolasCagePage, 'amazon'),
    (CnnArticlePage, 'cnn_article'),
]


def AddPagesToPageSet(
    page_set,
    shared_page_state_class=shared_page_state.SharedMobilePageState,
    name_func=lambda name: name,
    extra_browser_args=None):
  # Add pages with predefined classes that contain custom navigation logic.
  for page_class, page_name in PREDEFINED_PAGE_CLASSES:
    page_set.AddStory(
        page_class(
            page_set=page_set,
            shared_page_state_class=shared_page_state_class,
            name=name_func(page_name),
            extra_browser_args=extra_browser_args))

  # Add pages with custom tags.
  for page_url, page_name in FASTPATH_URLS:
    page_set.AddStory(
        KeyMobileSitesSmoothPage(
            url=page_url,
            page_set=page_set,
            shared_page_state_class=shared_page_state_class,
            name=name_func(page_name),
            tags=['fastpath'],
            extra_browser_args=extra_browser_args))

  # Why: Wikipedia page with a delayed scroll start
  page_set.AddStory(
      KeyMobileSitesSmoothPage(
          url='http://en.wikipedia.org/wiki/Wikipedia',
          page_set=page_set,
          shared_page_state_class=shared_page_state_class,
          name=name_func('wikipedia_delayed_scroll_start'),
          action_on_load_complete=True,
          extra_browser_args=extra_browser_args))

  # Add simple pages with no custom navigation logic or tags.
  for page_url, page_name in URLS_LIST:
    page_set.AddStory(
        KeyMobileSitesSmoothPage(
            url=page_url,
            page_set=page_set,
            shared_page_state_class=shared_page_state_class,
            name=name_func(page_name),
            extra_browser_args=extra_browser_args))


class KeyMobileSitesSmoothPageSet(story.StorySet):
  """ Key mobile sites with smooth interactions. """

  def __init__(self):
    super(KeyMobileSitesSmoothPageSet, self).__init__(
        archive_data_file='data/key_mobile_sites_smooth.json',
        cloud_storage_bucket=story.PARTNER_BUCKET)

    AddPagesToPageSet(self)
