# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets.system_health import platforms
from page_sets.system_health import story_tags
from page_sets.system_health import system_health_story

from page_sets.login_helpers import dropbox_login
from page_sets.login_helpers import facebook_login
from page_sets.login_helpers import google_login

from page_sets.helpers import override_online

from telemetry.util import js_template
from telemetry.util import wpr_modes


class _LoadingStory(system_health_story.SystemHealthStory):
  """Abstract base class for single-page System Health user stories."""
  ABSTRACT_STORY = True
  EXTRA_BROWSER_ARGUMENTS = []

  def __init__(self, story_set, take_memory_measurement):
    super(_LoadingStory,
          self).__init__(story_set,
                         take_memory_measurement,
                         extra_browser_args=self.EXTRA_BROWSER_ARGUMENTS)
    self.script_to_evaluate_on_commit = override_online.ALWAYS_ONLINE

  @classmethod
  def GenerateStoryDescription(cls):
    return 'Load %s' % cls.URL


################################################################################
# Search and e-commerce.
################################################################################
# TODO(petrcermak): Split these into 'portal' and 'shopping' stories.


class LoadGoogleStory2018(_LoadingStory):
  NAME = 'load:search:google:2018'
  URL = 'https://www.google.co.uk/search?q=pepper'
  TAGS = [story_tags.YEAR_2018]


class LoadBaiduStory2018(_LoadingStory):
  NAME = 'load:search:baidu:2018'
  URL = 'https://www.baidu.com/s?word=google'
  TAGS = [story_tags.INTERNATIONAL, story_tags.YEAR_2018]


class LoadYahooStory2018(_LoadingStory):
  NAME = 'load:search:yahoo:2018'
  # Use additional parameter to bypass consent screen.
  URL = 'https://search.yahoo.com/search;_ylt=?p=google&_guc_consent_skip=1541794498'
  TAGS = [story_tags.YEAR_2018]


class LoadAmazonDesktopStory2018(_LoadingStory):
  NAME = 'load:search:amazon:2018'
  URL = 'https://www.amazon.com/s/?field-keywords=pixel'
  TAGS = [story_tags.YEAR_2018]
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY


class LoadAmazonMobileStory2019(_LoadingStory):
  NAME = 'load:search:amazon:2019'
  URL = 'https://www.amazon.com/s/?field-keywords=pixel'
  TAGS = [story_tags.YEAR_2019]
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY


class LoadTaobaoDesktopStory2018(_LoadingStory):
  NAME = 'load:search:taobao:2018'
  URL = 'https://world.taobao.com/'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.INTERNATIONAL, story_tags.YEAR_2018]


class LoadFlipkartDesktop2018(_LoadingStory):
  NAME = 'load:search:flipkart:2018'
  URL = 'https://www.flipkart.com/search?q=sneakers'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.INTERNATIONAL, story_tags.YEAR_2018]


class LoadTaobaoMobileStory2019(_LoadingStory):
  NAME = 'load:search:taobao:2019'
  URL = 'http://m.intl.taobao.com/'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.INTERNATIONAL, story_tags.YEAR_2019]


class LoadYandexStory2018(_LoadingStory):
  NAME = 'load:search:yandex:2018'
  URL = 'https://yandex.ru/touchsearch?text=science'
  TAGS = [story_tags.INTERNATIONAL, story_tags.YEAR_2018]


class LoadEbayStory2018(_LoadingStory):
  NAME = 'load:search:ebay:2018'
  URL = 'https://www.ebay.com/sch/i.html?_nkw=headphones'
  TAGS = [story_tags.YEAR_2018]


class LoadNaverStory2023(_LoadingStory):
  NAME = 'load:search:naver:2023'
  URL = 'https://m.naver.com/'
  TAGS = [story_tags.INTERNATIONAL, story_tags.YEAR_2023]
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY

################################################################################
# Social networks.
################################################################################


class LoadTwitterMobileStory2019(_LoadingStory):
  NAME = 'load:social:twitter:2019'
  URL = 'https://www.twitter.com/nasa'
  TAGS = [story_tags.YEAR_2019]

  # Desktop version is already covered by
  # 'browse:social:twitter_infinite_scroll'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY


class LoadVkDesktopStory2018(_LoadingStory):
  NAME = 'load:social:vk:2018'
  URL = 'https://vk.com/sbeatles'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [
      story_tags.HEALTH_CHECK, story_tags.INTERNATIONAL, story_tags.YEAR_2018
  ]


class LoadInstagramDesktopStory2018(_LoadingStory):
  NAME = 'load:social:instagram:2018'
  URL = 'https://www.instagram.com/selenagomez/'
  TAGS = [story_tags.HEALTH_CHECK, story_tags.YEAR_2018]
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY


class LoadInstagramMobileStory2019(_LoadingStory):
  NAME = 'load:social:instagram:2019'
  URL = 'https://www.instagram.com/selenagomez/'
  TAGS = [story_tags.YEAR_2019]
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY


class LoadPinterestStory2019(_LoadingStory):
  NAME = 'load:social:pinterest:2019'
  URL = 'https://uk.pinterest.com/categories/popular/'
  TAGS = [story_tags.JAVASCRIPT_HEAVY, story_tags.YEAR_2019]


################################################################################
# News, discussion and knowledge portals and blogs.
################################################################################


class LoadBbcDesktopStory2018(_LoadingStory):
  NAME = 'load:news:bbc:2018'
  URL = 'https://www.bbc.co.uk/news'
  TAGS = [story_tags.YEAR_2018]
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY


class LoadBbcMobileStory2019(_LoadingStory):
  NAME = 'load:news:bbc:2019'
  URL = 'https://www.bbc.co.uk/news'
  TAGS = [story_tags.YEAR_2019]
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY


class LoadCnnStory2020(_LoadingStory):
  NAME = 'load:news:cnn:2020'
  URL = 'https://edition.cnn.com'
  TAGS = [
      story_tags.HEALTH_CHECK, story_tags.JAVASCRIPT_HEAVY, story_tags.YEAR_2020
  ]


class LoadFlipboardDesktopStory(_LoadingStory):
  NAME = 'load:news:flipboard'
  URL = 'https://flipboard.com/explore'
  TAGS = [story_tags.YEAR_2016]
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY


class LoadHackerNewsDesktopStory2018(_LoadingStory):
  NAME = 'load:news:hackernews:2018'
  URL = 'https://news.ycombinator.com'
  TAGS = [story_tags.YEAR_2018]
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY


class LoadNytimesDesktopStory2018(_LoadingStory):
  NAME = 'load:news:nytimes:2018'
  URL = 'http://www.nytimes.com'
  TAGS = [story_tags.YEAR_2018]
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY


class LoadNytimesMobileStory2019(_LoadingStory):
  NAME = 'load:news:nytimes:2019'
  URL = 'http://mobile.nytimes.com'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.YEAR_2019]


class LoadQqMobileStory2019(_LoadingStory):
  NAME = 'load:news:qq:2019'
  URL = 'https://xw.qq.com/?f=c_news'
  TAGS = [story_tags.INTERNATIONAL, story_tags.YEAR_2019]
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY


class LoadQqDesktopStory2018(_LoadingStory):
  NAME = 'load:news:qq:2018'
  URL = 'https://news.qq.com'
  TAGS = [story_tags.INTERNATIONAL, story_tags.YEAR_2018]
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY


class LoadRedditDesktopStory2018(_LoadingStory):
  NAME = 'load:news:reddit:2018'
  URL = 'https://www.reddit.com/r/news/top/?sort=top&t=week'
  TAGS = [story_tags.YEAR_2018]
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY


class LoadRedditMobileStory2019(_LoadingStory):
  NAME = 'load:news:reddit:2019'
  URL = 'https://www.reddit.com/r/news/top/?sort=top&t=week'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.YEAR_2019]


class LoadWashingtonPostMobileStory2019(_LoadingStory):
  NAME = 'load:news:washingtonpost:2019'
  URL = 'https://www.washingtonpost.com/pwa'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.YEAR_2019]
  _CONTINUE_FREE_BUTTON_SELECTOR = '.continue-btn.button.free'
  _ACCEPT_GDPR_SELECTOR = '.agree-ckb'
  _CONTINUE_TO_SITE_SELECTOR = '.continue-btn.button.accept-consent'

  def _DidLoadDocument(self, action_runner):
    # Close the popup window. On Nexus 9 (and probably other tables) the popup
    # window does not have a "Close" button, instead it has only a "Send link
    # to phone" button. So on tablets we run with the popup window open. The
    # popup is transparent, so this is mostly an aesthetical issue.
    has_button = action_runner.EvaluateJavaScript(
        '!!document.querySelector({{ selector }})',
        selector=self._CONTINUE_FREE_BUTTON_SELECTOR)
    if has_button:
      action_runner.ClickElement(selector=self._CONTINUE_FREE_BUTTON_SELECTOR)
      action_runner.ScrollPageToElement(selector=self._ACCEPT_GDPR_SELECTOR)
      action_runner.ClickElement(selector=self._ACCEPT_GDPR_SELECTOR)
      element_function = js_template.Render(
        'document.querySelectorAll({{ selector }})[{{ index }}]',
        selector=self._CONTINUE_TO_SITE_SELECTOR, index=0)
      action_runner.ClickElement(element_function=element_function)


class LoadWikipediaStory2018(_LoadingStory):
  NAME = 'load:news:wikipedia:2018'
  URL = 'https://en.wikipedia.org/wiki/Science'
  TAGS = [story_tags.EMERGING_MARKET, story_tags.YEAR_2018]


class LoadIrctcStory2019(_LoadingStory):
  NAME = 'load:news:irctc:2019'
  URL = 'https://www.irctc.co.in'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.EMERGING_MARKET, story_tags.YEAR_2019]

  def _Login(self, action_runner):
    # There is an error on replay that pops up the first time. If we
    # navigate again, the error disappears.
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()
    action_runner.Navigate(self.URL)
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()

################################################################################
# Audio, images, and video.
################################################################################


class LoadYouTubeStory2018(_LoadingStory):
  # No way to disable autoplay on desktop.
  NAME = 'load:media:youtube:2018'
  URL = 'https://www.youtube.com/watch?v=QGfhS1hfTWw&autoplay=false'
  TAGS = [
      story_tags.HEALTH_CHECK, story_tags.EMERGING_MARKET, story_tags.YEAR_2018
  ]


class LoadYouTubeLivingRoomStory2020(_LoadingStory):
  NAME = 'load:media:youtubelivingroom:2020'
  URL = 'https://www.youtube.com/tv#/watch?v=AIyonw6LEOs'
  TAGS = [story_tags.HEALTH_CHECK, story_tags.YEAR_2020]
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY


class LoadDailymotionStory2019(_LoadingStory):
  NAME = 'load:media:dailymotion:2019'
  URL = ('https://www.dailymotion.com/video/x7paozv')
  TAGS = [story_tags.YEAR_2019]


class LoadGoogleImagesStory2018(_LoadingStory):
  NAME = 'load:media:google_images:2018'
  URL = 'https://www.google.co.uk/search?tbm=isch&q=love'
  TAGS = [story_tags.YEAR_2018]


class LoadSoundCloudStory2018(_LoadingStory):
  # No way to disable autoplay on desktop. Album artwork doesn't load due to
  NAME = 'load:media:soundcloud:2018'
  URL = 'https://soundcloud.com/lifeofdesiigner/desiigner-panda'
  TAGS = [story_tags.YEAR_2018]


class Load9GagDesktopStory(_LoadingStory):
  NAME = 'load:media:9gag'
  URL = 'https://www.9gag.com/'
  TAGS = [story_tags.YEAR_2016]
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY


class LoadImgurStory2018(_LoadingStory):
  NAME = 'load:media:imgur:2018'
  URL = 'http://imgur.com/gallery/5UlBN'
  TAGS = [story_tags.YEAR_2018]


class LoadFlickrStory2018(_LoadingStory):
  NAME = 'load:media:flickr:2018'
  URL = 'https://www.flickr.com/photos/tags/noiretblanc'
  TAGS = [story_tags.YEAR_2018]


class LoadFacebookPhotosMobileStory2019(_LoadingStory):
  """Load a page of rihanna's facebook with a photo."""
  NAME = 'load:media:facebook_photos:2019'
  URL = (
      'https://m.facebook.com/rihanna/photos/a.207477806675/10156574885461676/?type=3&source=54&ref=page_internal')
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.EMERGING_MARKET, story_tags.YEAR_2019]


class LoadFacebookPhotosDesktopStory2018(_LoadingStory):
  """Load a page of rihanna's facebook with a photo."""
  NAME = 'load:media:facebook_photos:2018'
  URL = (
    'https://www.facebook.com/rihanna/photos/pb.10092511675.-2207520000.1541795576./10155941787036676/?type=3&theater')
  TAGS = [story_tags.YEAR_2018]
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY


class _FacebookDesktopStory(_LoadingStory):
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY

  # Page event queries.
  VISUAL_COMPLETION_EVENT = '''
    (window.__telemetry_observed_page_events.has(
        "telemetry:reported_by_page:viewable"))
  '''

  # the reported_by_page:* metric.
  EVENTS_REPORTED_BY_PAGE = '''
    window.__telemetry_reported_page_events = {
      'VisuallyComplete(INITIAL_LOAD)': 'telemetry:reported_by_page:viewable',
    };
  '''

  # Patch performance.measure to get notified about metrics
  PERFORMANCE_MEASURE_PATCH = '''
    window.__telemetry_observed_page_events = new Set();
    (function () {
      let reported = window.__telemetry_reported_page_events;
      let observed = window.__telemetry_observed_page_events;
      let performance_measure = window.performance.measure;

      window.performance.measure = function (label, options) {
        performance_measure.call(window.performance, label, options);
        if (reported.hasOwnProperty(label)) {
         performance_measure.call(window.performance, reported[label], options);
         observed.add(reported[label]);
        }
      }

    })();
  '''

  def __init__(self, story_set, take_memory_measurement):
    super(_FacebookDesktopStory, self).__init__(story_set,
                                                take_memory_measurement)
    self.script_to_evaluate_on_commit += "\n"
    self.script_to_evaluate_on_commit += js_template.Render(
        '''{{@events_reported_by_page}}
        {{@performance_measure}}''',
        events_reported_by_page=self.EVENTS_REPORTED_BY_PAGE,
        performance_measure=self.PERFORMANCE_MEASURE_PATCH)

  def _DidLoadDocument(self, action_runner):
    action_runner.WaitForJavaScriptCondition(self.VISUAL_COMPLETION_EVENT)


class LoadFacebookPhotosDesktopStory2020(_FacebookDesktopStory):
  """Load a page of rihanna's facebook with a photo."""
  NAME = 'load:media:facebook_photos:desktop:2020'
  URL = (
      'https://www.facebook.com/rihanna/photos/pb.10092511675.-2207520000.1541795576./10155941787036676/?type=3&theater'
  )
  TAGS = [story_tags.YEAR_2020]

  def _Login(self, action_runner):
    facebook_login.LoginWithDesktopSite(action_runner, 'facebook4')


class LoadFacebookFeedDesktopStory2020(_FacebookDesktopStory):
  """Load facebook main page"""
  NAME = 'load:media:facebook_feed:desktop:2020'
  URL = 'https://www.facebook.com/'
  TAGS = [story_tags.YEAR_2020]

  def _Login(self, action_runner):
    facebook_login.LoginWithDesktopSite(action_runner, 'facebook4')


class LoadFacebookPhotosMobileStory2020(_LoadingStory):
  """Load a page of rihanna's facebook with a photo."""
  NAME = 'load:media:facebook_photos:mobile:2020'
  URL = (
      'https://m.facebook.com/rihanna/photos/pb.10092511675.-2207520000.1541795576./10155941787036676/'
  )
  TAGS = [story_tags.YEAR_2020]
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY

  def _Login(self, action_runner):
    facebook_login.LoginWithMobileSite(action_runner, 'facebook4')


class LoadFacebookFeedMobileStory2020(_LoadingStory):
  """Load a page of national park"""
  NAME = 'load:media:facebook_feed:mobile:2020'
  URL = ('https://www.facebook.com/')
  TAGS = [story_tags.YEAR_2020]
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY

  def _Login(self, action_runner):
    facebook_login.LoginWithMobileSite(action_runner, 'facebook4')


################################################################################
# Online tools (documents, emails, storage, ...).
################################################################################


class LoadDocsStory2019(_LoadingStory):
  """Load a typical google doc page (2019)."""
  NAME = 'load:tools:docs:2019'
  URL = (
      'https://docs.google.com/document/d/1GvzDP-tTLmJ0myRhUAfTYWs3ZUFilUICg8psNHyccwQ/edit?usp=sharing')
  TAGS = [story_tags.YEAR_2019]


class _LoadGmailBaseStory(_LoadingStory):
  NAME = 'load:tools:gmail'
  URL = 'https://mail.google.com/mail/'
  ABSTRACT_STORY = True

  def _Login(self, action_runner):
    google_login.LoginGoogleAccount(action_runner, 'googletest')

    # Navigating to https://mail.google.com immediately leads to an infinite
    # redirection loop due to a bug in WPR (see
    # https://github.com/chromium/web-page-replay/issues/70). We therefore first
    # navigate to a sub-URL to set up the session and hit the resulting
    # redirection loop. Afterwards, we can safely navigate to
    # https://mail.google.com.
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()
    action_runner.Navigate(
        'https://mail.google.com/mail/mu/mp/872/trigger_redirection_loop')
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()


class LoadGmailStory2019(_LoadingStory):
  NAME = 'load:tools:gmail:2019'
  # Needs to be http and not https.
  URL = 'http://mail.google.com/'
  TAGS = [story_tags.HEALTH_CHECK, story_tags.YEAR_2019]
  SKIP_LOGIN = False
  EXTRA_BROWSER_ARGUMENTS = ['--allow-browser-signin=false']

  def _Login(self, action_runner):
    if self.wpr_mode in [wpr_modes.WPR_OFF, wpr_modes.WPR_RECORD]:
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


class LoadChatStory2020(_LoadingStory):
  NAME = 'load:tools:chat:2020'
  # Needs to be http and not https.
  URL = 'http://chat.google.com/'
  TAGS = [story_tags.YEAR_2020]
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  SKIP_LOGIN = False
  EXTRA_BROWSER_ARGUMENTS = ['--allow-browser-signin=false']

  def _Login(self, action_runner):
    if self.wpr_mode in [wpr_modes.WPR_OFF, wpr_modes.WPR_RECORD]:
      google_login.LoginWithLoginUrl(action_runner, self.URL)
    else:
      google_login.NewLoginGoogleAccount(action_runner, 'chatfeature')

    action_runner.tab.WaitForDocumentReadyStateToBeComplete()



class LoadStackOverflowStory2018(_LoadingStory):
  """Load a typical question & answer page of stackoverflow.com"""
  NAME = 'load:tools:stackoverflow:2018'
  URL = (
      'https://stackoverflow.com/questions/36827659/compiling-an-application-for-use-in-highly-radioactive-environments')
  TAGS = [story_tags.YEAR_2018]


class LoadDropboxStory2019(_LoadingStory):
  NAME = 'load:tools:dropbox:2019'
  URL = 'https://www.dropbox.com/'
  TAGS = [story_tags.YEAR_2019]
  # Desktop fails to log in
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  SKIP_LOGIN = False

  def _Login(self, action_runner):
    dropbox_login.LoginAccount(action_runner, 'dropbox')


class LoadWeatherStory2019(_LoadingStory):
  NAME = 'load:tools:weather:2019'
  URL = 'https://weather.com/en-GB/weather/today/l/USCA0286:1:US'
  TAGS = [
      story_tags.HEALTH_CHECK, story_tags.JAVASCRIPT_HEAVY, story_tags.YEAR_2019
  ]


class LoadDriveStory2019(_LoadingStory):
  NAME = 'load:tools:drive:2019'
  URL = 'https://drive.google.com/drive/my-drive'
  TAGS = [story_tags.JAVASCRIPT_HEAVY, story_tags.YEAR_2019]
  EXTRA_BROWSER_ARGUMENTS = ['--allow-browser-signin=false']

  def _Login(self, action_runner):
    if self.wpr_mode in [wpr_modes.WPR_OFF, wpr_modes.WPR_RECORD]:
      google_login.LoginWithLoginUrl(action_runner, self.URL)
    else:
      google_login.NewLoginGoogleAccount(action_runner, 'googletest')


################################################################################
# In-browser games (HTML5 and Flash).
################################################################################


class LoadBubblesStory2020(_LoadingStory):
  """Load "smarty bubbles" game on famobi.com"""
  NAME = 'load:games:bubbles:2020'
  URL = (
      'https://games.cdn.famobi.com/html5games/s/smarty-bubbles/v010/?fg_domain=play.famobi.com&fg_uid=d8f24956-dc91-4902-9096-a46cb1353b6f&fg_pid=4638e320-4444-4514-81c4-d80a8c662371&fg_beat=620')
  TAGS = [story_tags.YEAR_2020]


class LoadLazorsStory(_LoadingStory):
  NAME = 'load:games:lazors'
  # Using "https://" hangs and shows "This site can't be reached".
  URL = 'http://www8.games.mobi/games/html5/lazors/lazors.html'
  TAGS = [story_tags.YEAR_2016]


class LoadSpyChaseStory2018(_LoadingStory):
  NAME = 'load:games:spychase:2018'
  # Using "https://" shows "Your connection is not private".
  URL = 'http://playstar.mobi/games/spychase/index.php'
  TAGS = [story_tags.YEAR_2018]

  def _DidLoadDocument(self, action_runner):
    # The background of the game canvas is set when the "Tap screen to play"
    # caption is displayed.
    action_runner.WaitForJavaScriptCondition(
        'document.querySelector("#game canvas").style.background !== ""')


class LoadMiniclipStory2018(_LoadingStory):
  NAME = 'load:games:miniclip:2018'
  # Using "https://" causes "404 Not Found" during WPR recording.
  URL = 'http://www.miniclip.com/games/en/'
  TAGS = [story_tags.YEAR_2018]
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY  # Requires Flash.


class LoadAlphabettyStory2018(_LoadingStory):
  NAME = 'load:games:alphabetty:2018'
  URL = 'https://king.com/play/alphabetty'
  TAGS = [story_tags.YEAR_2018]
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY  # Requires Flash.
