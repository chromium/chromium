# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets.system_health import platforms
from page_sets.system_health import story_tags
from page_sets.system_health import system_health_story

from page_sets.login_helpers import dropbox_login
from page_sets.login_helpers import google_login

from telemetry.util import js_template


class _LoadingStory(system_health_story.SystemHealthStory):
  """Abstract base class for single-page System Health user stories."""
  ABSTRACT_STORY = True

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
  TAGS = [story_tags.INTERNATIONAL, story_tags.HEALTH_CHECK,
          story_tags.YEAR_2018]


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


class LoadTaobaoMobileStory(_LoadingStory):
  NAME = 'load:search:taobao'
  # "ali_trackid" in the URL suppresses "Download app" interstitial.
  URL = 'http://m.intl.taobao.com/?ali_trackid'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.INTERNATIONAL, story_tags.HEALTH_CHECK,
          story_tags.YEAR_2016]


class LoadYandexStory2018(_LoadingStory):
  NAME = 'load:search:yandex:2018'
  URL = 'https://yandex.ru/touchsearch?text=science'
  TAGS = [story_tags.INTERNATIONAL, story_tags.YEAR_2018]


class LoadEbayStory2018(_LoadingStory):
  NAME = 'load:search:ebay:2018'
  URL = 'https://www.ebay.com/sch/i.html?_nkw=headphones'
  TAGS = [story_tags.HEALTH_CHECK, story_tags.YEAR_2018]


################################################################################
# Social networks.
################################################################################


class LoadTwitterStory(_LoadingStory):
  NAME = 'load:social:twitter'
  URL = 'https://www.twitter.com/nasa'
  TAGS = [story_tags.YEAR_2016]

  # Desktop version is already covered by
  # 'browse:social:twitter_infinite_scroll'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY

class LoadTwitterMoibleStory2019(_LoadingStory):
  NAME = 'load:social:twitter:2019'
  URL = 'https://www.twitter.com/nasa'
  TAGS = [story_tags.YEAR_2019]

  # Desktop version is already covered by
  # 'browse:social:twitter_infinite_scroll'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY

class LoadVkStory(_LoadingStory):
  NAME = 'load:social:vk'
  URL = 'https://vk.com/sbeatles'
  # Due to the deterministic date injected by WPR (February 2008), the cookie
  # set by https://vk.com immediately expires, so the page keeps refreshing
  # indefinitely on mobile
  # (see https://github.com/chromium/web-page-replay/issues/71).
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.INTERNATIONAL, story_tags.YEAR_2016]


class LoadVkDesktopStory2018(_LoadingStory):
  NAME = 'load:social:vk:2018'
  URL = 'https://vk.com/sbeatles'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  TAGS = [story_tags.INTERNATIONAL, story_tags.YEAR_2018]


class LoadInstagramDesktopStory2018(_LoadingStory):
  NAME = 'load:social:instagram:2018'
  URL = 'https://www.instagram.com/selenagomez/'
  TAGS = [story_tags.YEAR_2018]
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY

class LoadInstagramMobileStory2019(_LoadingStory):
  NAME = 'load:social:instagram:2019'
  URL = 'https://www.instagram.com/selenagomez/'
  TAGS = [story_tags.YEAR_2019]
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY

class LoadPinterestStory(_LoadingStory):
  NAME = 'load:social:pinterest'
  URL = 'https://uk.pinterest.com/categories/popular/'
  TAGS = [story_tags.JAVASCRIPT_HEAVY, story_tags.YEAR_2016]
  # Mobile story is already covered by
  # 'browse:social:pinterest_infinite_scroll'.
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY


################################################################################
# News, discussion and knowledge portals and blogs.
################################################################################


class LoadBbcDesktopStory2018(_LoadingStory):
  NAME = 'load:news:bbc:2018'
  URL = 'https://www.bbc.co.uk/news'
  TAGS = [story_tags.YEAR_2018]
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY


class LoadCnnStory2018(_LoadingStory):
  NAME = 'load:news:cnn:2018'
  URL = 'https://edition.cnn.com'
  TAGS = [story_tags.JAVASCRIPT_HEAVY, story_tags.YEAR_2018]


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


class LoadNytimesMobileStory(_LoadingStory):
  NAME = 'load:news:nytimes'
  URL = 'http://mobile.nytimes.com'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.HEALTH_CHECK, story_tags.YEAR_2016]

class LoadNytimesMobileStory2019(_LoadingStory):
  NAME = 'load:news:nytimes:2019'
  URL = 'http://mobile.nytimes.com'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.HEALTH_CHECK, story_tags.YEAR_2019]

class LoadQqMobileStory(_LoadingStory):
  NAME = 'load:news:qq'
  # Using "https://" hangs and shows "This site can't be reached".
  URL = 'http://news.qq.com'
  TAGS = [story_tags.INTERNATIONAL, story_tags.YEAR_2016]

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


class LoadRedditMobileStory(_LoadingStory):
  NAME = 'load:news:reddit'
  URL = 'https://www.reddit.com/r/news/top/?sort=top&t=week'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.HEALTH_CHECK, story_tags.YEAR_2016]

class LoadRedditMobileStory2019(_LoadingStory):
  NAME = 'load:news:reddit:2019'
  URL = 'https://www.reddit.com/r/news/top/?sort=top&t=week'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.HEALTH_CHECK, story_tags.YEAR_2019]

class LoadWashingtonPostMobileStory(_LoadingStory):
  NAME = 'load:news:washingtonpost'
  URL = 'https://www.washingtonpost.com/pwa'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.HEALTH_CHECK, story_tags.YEAR_2016]
  _CLOSE_BUTTON_SELECTOR = '.close'

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

class LoadWashingtonPostMobileStory2019(_LoadingStory):
  NAME = 'load:news:washingtonpost:2019'
  URL = 'https://www.washingtonpost.com/pwa'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.HEALTH_CHECK, story_tags.YEAR_2019]
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


class LoadIrctcStory(_LoadingStory):
  NAME = 'load:news:irctc'
  URL = 'https://www.irctc.co.in'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.EMERGING_MARKET, story_tags.HEALTH_CHECK,
          story_tags.YEAR_2016]


################################################################################
# Audio, images, and video.
################################################################################


class LoadYouTubeStory2018(_LoadingStory):
  # No way to disable autoplay on desktop.
  NAME = 'load:media:youtube:2018'
  URL = 'https://www.youtube.com/watch?v=QGfhS1hfTWw&autoplay=false'
  TAGS = [story_tags.EMERGING_MARKET, story_tags.HEALTH_CHECK,
          story_tags.YEAR_2018]


class LoadDailymotionStory(_LoadingStory):
  # The side panel with related videos doesn't show on desktop due to
  # https://github.com/chromium/web-page-replay/issues/74.
  NAME = 'load:media:dailymotion'
  URL = (
      'https://www.dailymotion.com/video/x489k7d_street-performer-shows-off-slinky-skills_fun?autoplay=false')
  TAGS = [story_tags.YEAR_2016]


class LoadGoogleImagesStory2018(_LoadingStory):
  NAME = 'load:media:google_images:2018'
  URL = 'https://www.google.co.uk/search?tbm=isch&q=love'
  TAGS = [story_tags.HEALTH_CHECK, story_tags.YEAR_2018]


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
  TAGS = [story_tags.HEALTH_CHECK, story_tags.YEAR_2018]


class LoadFlickrStory2018(_LoadingStory):
  NAME = 'load:media:flickr:2018'
  URL = 'https://www.flickr.com/photos/tags/noiretblanc'
  TAGS = [story_tags.YEAR_2018]


class LoadFacebookPhotosMobileStory(_LoadingStory):
  """Load a page of rihanna's facebook with a photo."""
  NAME = 'load:media:facebook_photos'
  URL = (
      'https://m.facebook.com/rihanna/photos/a.207477806675.138795.10092511675/10153911739606676/?type=3&source=54&ref=page_internal')
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.EMERGING_MARKET, story_tags.YEAR_2016]

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


################################################################################
# Online tools (documents, emails, storage, ...).
################################################################################


class LoadDocsStory(_LoadingStory):
  """Load a typical google doc page."""
  NAME = 'load:tools:docs'
  URL = (
      'https://docs.google.com/document/d/1GvzDP-tTLmJ0myRhUAfTYWs3ZUFilUICg8psNHyccwQ/edit?usp=sharing')
  TAGS = [story_tags.HEALTH_CHECK, story_tags.YEAR_2016]


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


class LoadGmailDesktopStory(_LoadGmailBaseStory):
  TAGS = [story_tags.YEAR_2016]
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY

  def _DidLoadDocument(self, action_runner):
    # Wait until the UI loads.
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("loading").style.display === "none"')


class LoadGmailMobileStory(_LoadGmailBaseStory):
  TAGS = [story_tags.YEAR_2016]
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  # TODO(crbug.com/862077): Story breaks if login is skipped during replay.
  SKIP_LOGIN = False

  def _DidLoadDocument(self, action_runner):
    # Wait until the UI loads.
    action_runner.WaitForElement('#apploadingdiv')
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("apploadingdiv").style.height === "0px"')

class LoadStackOverflowStory2018(_LoadingStory):
  """Load a typical question & answer page of stackoverflow.com"""
  NAME = 'load:tools:stackoverflow:2018'
  URL = (
      'https://stackoverflow.com/questions/36827659/compiling-an-application-for-use-in-highly-radioactive-environments')
  TAGS = [story_tags.HEALTH_CHECK, story_tags.YEAR_2018]


class LoadDropboxStory(_LoadingStory):
  NAME = 'load:tools:dropbox'
  URL = 'https://www.dropbox.com'
  TAGS = [story_tags.YEAR_2016]

  def _Login(self, action_runner):
    dropbox_login.LoginAccount(action_runner, 'dropbox')


class LoadWeatherStory(_LoadingStory):
  NAME = 'load:tools:weather'
  URL = 'https://weather.com/en-GB/weather/today/l/USCA0286:1:US'
  TAGS = [story_tags.JAVASCRIPT_HEAVY, story_tags.YEAR_2016]


class LoadDriveStory(_LoadingStory):
  NAME = 'load:tools:drive'
  URL = 'https://drive.google.com/drive/my-drive'
  TAGS = [story_tags.JAVASCRIPT_HEAVY, story_tags.YEAR_2016]

  def _Login(self, action_runner):
    google_login.LoginGoogleAccount(action_runner, 'googletest')


################################################################################
# In-browser games (HTML5 and Flash).
################################################################################


class LoadBubblesStory(_LoadingStory):
  """Load "smarty bubbles" game on famobi.com"""
  NAME = 'load:games:bubbles'
  URL = (
      'https://games.cdn.famobi.com/html5games/s/smarty-bubbles/v010/?fg_domain=play.famobi.com&fg_uid=d8f24956-dc91-4902-9096-a46cb1353b6f&fg_pid=4638e320-4444-4514-81c4-d80a8c662371&fg_beat=620')
  TAGS = [story_tags.HEALTH_CHECK, story_tags.YEAR_2016]

  def _DidLoadDocument(self, action_runner):
    # The #logo element is removed right before the main menu is displayed.
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("logo") === null')

class LoadBubblesStory2019(_LoadingStory):
  """Load "smarty bubbles" game on famobi.com"""
  NAME = 'load:games:bubbles:2019'
  URL = (
      'https://games.cdn.famobi.com/html5games/s/smarty-bubbles/v010/?fg_domain=play.famobi.com&fg_uid=d8f24956-dc91-4902-9096-a46cb1353b6f&fg_pid=4638e320-4444-4514-81c4-d80a8c662371&fg_beat=620')
  TAGS = [story_tags.HEALTH_CHECK, story_tags.YEAR_2019]

class LoadLazorsStory(_LoadingStory):
  NAME = 'load:games:lazors'
  # Using "https://" hangs and shows "This site can't be reached".
  URL = 'http://www8.games.mobi/games/html5/lazors/lazors.html'
  TAGS = [story_tags.HEALTH_CHECK, story_tags.YEAR_2016]


class LoadSpyChaseStory2018(_LoadingStory):
  NAME = 'load:games:spychase:2018'
  # Using "https://" shows "Your connection is not private".
  URL = 'http://playstar.mobi/games/spychase/index.php'
  TAGS = [story_tags.HEALTH_CHECK, story_tags.YEAR_2018]

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
  TAGS = [story_tags.YEAR_2016]
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY  # Requires Flash.
