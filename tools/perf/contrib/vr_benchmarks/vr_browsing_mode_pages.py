# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

from telemetry import page
from telemetry import story
from telemetry.page import shared_page_state
from devil.android.sdk import intent  # pylint: disable=import-error
from contrib.vr_benchmarks import shared_vr_page_state as vr_state
from contrib.vr_benchmarks.vr_sample_page import VrSamplePage
from contrib.vr_benchmarks.vr_story_set import VrStorySet
from page_sets import key_mobile_sites_smooth as smooth_sites


# List of URLs taken from the legacy memory.top_10_mobile benchmark.
URL_LIST = [
    # Why: #1 (Alexa) most visited page worldwide, picked a reasonable
    # search term
    'https://www.google.co.uk/#hl=en&q=science',
    # Why: #2 (Alexa) most visited page worldwide, picked the most liked
    # page
    'https://m.facebook.com/rihanna',
    # Why: #3 (Alexa) most visited page worldwide, picked a reasonable
    # search term
    'http://m.youtube.com/results?q=science',
    # Why: #4 (Alexa) most visited page worldwide, picked a reasonable search
    # term
    'http://search.yahoo.com/search;_ylt=?p=google',
    # Why: #5 (Alexa) most visited page worldwide, picked a reasonable search
    # term
    'http://www.baidu.com/s?word=google',
    # Why: #6 (Alexa) most visited page worldwide, picked a reasonable page
    'http://en.m.wikipedia.org/wiki/Science',
    # Why: #10 (Alexa) most visited page worldwide, picked the most followed
    # user
    'https://mobile.twitter.com/justinbieber?skip_interstitial=true',
    # Why: #11 (Alexa) most visited page worldwide, picked a reasonable
    # page
    'http://www.amazon.com/gp/aw/s/?k=nexus',
    # Why: #13 (Alexa) most visited page worldwide, picked the first real
    # page
    'http://m.intl.taobao.com/group-purchase.html',
    # Why: #18 (Alexa) most visited page worldwide, picked a reasonable
    # search term
    'http://yandex.ru/touchsearch?text=science',
]


def _EnterVrViaNfc(current_page, action_runner):
  def isNfcAppReady(android_app):
    del android_app
    # TODO(tiborg): Find a way to tell if the NFC app ran successfully.
    return True

  # Enter VR by simulating an NFC tag scan.
  current_page.platform.LaunchAndroidApplication(
      start_intent=intent.Intent(
          component=
          'org.chromium.chrome.browser.vr.nfc_apk/.SimNfcActivity'),
      is_app_ready_predicate=isNfcAppReady,
      app_has_webviews=False).Close()

  # Wait until Chrome is settled in VR.
  # TODO(tiborg): Implement signal indicating that Chrome went into VR
  # Browsing Mode. Wait times are flaky.
  action_runner.Wait(2)


def _EnterVrViaNfcWithMemory(current_page, action_runner):
  _EnterVrViaNfc(current_page, action_runner)

  # MeasureMemory() waits for 10 seconds before measuring memory, which is
  # long enough for us to collect our other data, so no additional sleeps
  # necessary.
  action_runner.MeasureMemory(True)


class Simple2dStillPage(VrSamplePage):
  """A simple 2D page without user interactions."""

  def __init__(self, page_set, sample_page='index'):
    super(Simple2dStillPage, self).__init__(
         sample_page=sample_page, page_set=page_set)

  def RunPageInteractions(self, action_runner):
    _EnterVrViaNfcWithMemory(self, action_runner)


class VrBrowsingModeWprPage(page.Page):
  """Class for running a VR browsing story on a WPR page."""

  def __init__(self, page_set, url, name, extra_browser_args=None):
    """
    Args:
      page_set: The StorySet the VrBrowsingModeWprPage is being added to
      url: The URL to navigate to for the story
      name: The name of the story
      extra_browser_args: Extra browser args that are simply forwarded to
          page.Page
    """
    super(VrBrowsingModeWprPage, self).__init__(
        url=url,
        page_set=page_set,
        name=name,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=vr_state.AndroidSharedVrPageState)
    self._shared_page_state = None

  def RunPageInteractions(self, action_runner):
    _EnterVrViaNfcWithMemory(self, action_runner)

  def Run(self, shared_state):
    self._shared_page_state = shared_state
    super(VrBrowsingModeWprPage, self).Run(shared_state)

  @property
  def platform(self):
    return self._shared_page_state.platform


class VrBrowsingModePageSet(VrStorySet):
  """Pageset for VR Browsing Mode tests on sample pages."""

  def __init__(self, use_fake_pose_tracker=True):
    super(VrBrowsingModePageSet, self).__init__(
        use_fake_pose_tracker=use_fake_pose_tracker)
    self.AddStory(Simple2dStillPage(self))


class VrBrowsingModeWprPageSet(VrStorySet):
  """Pageset for VR browsing mode on WPR recordings of live sites."""

  def __init__(self, use_fake_pose_tracker=True):
    super(VrBrowsingModeWprPageSet, self).__init__(
        archive_data_file='data/memory_top_10_mobile.json',
        cloud_storage_bucket=story.PARTNER_BUCKET,
        use_fake_pose_tracker=use_fake_pose_tracker)

    for url in URL_LIST:
      name = re.sub(r'\W+', '_', url)
      self.AddStory(VrBrowsingModeWprPage(self, url, name))


class VrBrowsingModeWprSmoothnessPage(VrBrowsingModeWprPage):
  """Hybrid of VrBrowsingModeWprPage and KeyMobileSitesSmoothPage."""
  def __init__(self, page_set, url, name, extra_browser_args=None, **kwargs):
    self._page_impl = smooth_sites.KeyMobileSitesSmoothPage(
        url=url, page_set=page_set, name=name,
        extra_browser_args=extra_browser_args, **kwargs)
    super(VrBrowsingModeWprSmoothnessPage, self).__init__(
        url=url,
        page_set=page_set,
        name=name,
        extra_browser_args=extra_browser_args)

  def RunPageInteractions(self, action_runner):
    _EnterVrViaNfc(self, action_runner)
    self._page_impl.RunPageInteractions(action_runner)


class VrBrowsingModeWprSmoothnessPageWrapper(VrBrowsingModeWprPage):
  """Wrapper class for running special pages in VR.

  A number of pre-existing pages used for scroll testing require special
  navigation and interaction steps, as opposed to just loading some URL and
  scrolling. Since we need to inherit from a page that exposes the shared state
  and/or platform during a story run, we can't just directly inherit from them.

  This way, we're able to inherit from a VR page that exposes the shared state,
  but re-use the navigation/interaction code from the non-VR pages while
  avoiding things like multiple inheritance.
  """

  def __init__(self, page_set, name, page_class, extra_browser_args=None):
    self._page_impl = page_class(
        page_set=page_set, name=name, extra_browser_args=None)
    super(VrBrowsingModeWprSmoothnessPageWrapper, self).__init__(
        url=self._page_impl.url,
        page_set=page_set,
        name=name,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    self._page_impl.RunNavigateSteps(action_runner)

  def RunPageInteractions(self, action_runner):
    _EnterVrViaNfc(self, action_runner)
    self._page_impl.RunPageInteractions(action_runner)


class VimeoPage(smooth_sites.KeyMobileSitesSmoothPage):
  """Page created in the same manner as other smoothness pages, but only for VR.

  Why: Video is a large use case for the VR browser, but Vimeo isn't popular
       enough to warrant putting in the normal key_mobile_sites_smooth page set.
  """

  def __init__(self, page_set, name='', extra_browser_args=None,
      shared_page_state_class=shared_page_state.SharedMobilePageState):
    super(VimeoPage, self).__init__(
        url='https://vimeo.com/search?q=Vr',
        page_set=page_set,
        name=name,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=shared_page_state_class)

  # Make sure we have enough results loaded to fully scroll.
  def RunNavigateSteps(self, action_runner):
    super(VimeoPage, self).RunNavigateSteps(action_runner)
    action_runner.ScrollPage()
    action_runner.ScrollPage(direction='up')


class VrBrowsingModeWprSmoothnessPageSet(VrStorySet):
  """Copy of KeyMobileSitesSmoothpageSet, but in the VR browser."""

  def __init__(self):
    super(VrBrowsingModeWprSmoothnessPageSet, self).__init__(
        archive_data_file='data/key_mobile_sites/key_mobile_sites_smooth.json',
        cloud_storage_bucket=story.PARTNER_BUCKET)

    # Add pages that require special navigation or interaction code.
    page_classes = [
      (smooth_sites.CapitolVolkswagenPage, 'capitolvolkswagen'),
      (smooth_sites.TheVergeArticlePage, 'theverge_article'),
      (smooth_sites.CnnArticlePage, 'cnn_article'),
      (smooth_sites.FacebookPage, 'facebook'),
      (smooth_sites.YoutubeMobilePage, 'youtube'),
      (smooth_sites.GoogleNewsMobilePage, 'google_news'),
      (smooth_sites.LinkedInPage, 'linkedin'),
      (smooth_sites.WowwikiPage, 'wowwiki'),
      (smooth_sites.AmazonNicolasCagePage, 'amazon'),
      (VimeoPage, 'vimeo'),
    ]
    for page_class, name in page_classes:
      self.AddStory(VrBrowsingModeWprSmoothnessPageWrapper(
          page_set=self, page_class=page_class, name=name))

    # Add pages with custom tags.
    for url, name in smooth_sites.FASTPATH_URLS:
      self.AddStory(VrBrowsingModeWprSmoothnessPage(
          url=url, page_set=self, name=name, tags=['fastpath']))

    # Add normal pages.
    for url, name in smooth_sites.URLS_LIST:
      self.AddStory(VrBrowsingModeWprSmoothnessPage(
          url=url, page_set=self, name=name))
