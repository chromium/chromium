# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets.system_health import platforms
from page_sets.system_health import story_tags
from page_sets.system_health import system_health_story
from page_sets.system_health import loading_stories
from page_sets.system_health import browsing_stories

_WAIT_FOR_VIDEO_SECONDS = 5

class _BackgroundStory(system_health_story.SystemHealthStory):
  """Abstract base class for background stories

  As in _LoadingStory except it puts the browser into the
  background before measuring.
  """
  ABSTRACT_STORY = True
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY

  def _Measure(self, action_runner):
    action_runner.tab.browser.Background()
    super(_BackgroundStory, self)._Measure(action_runner)

  @classmethod
  def GenerateStoryDescription(cls):
    return 'Load %s, then put the browser into the background.' % cls.URL


class BackgroundGoogleStory2019(_BackgroundStory):
  NAME = 'background:search:google:2019'
  URL = 'https://www.google.co.uk/#q=tom+cruise+movies'
  TAGS = [story_tags.YEAR_2019]

  def _DidLoadDocument(self, action_runner):
    # Activte the immersive movie browsing experience
    action_runner.WaitForElement(selector='.knHJyb')
    action_runner.ScrollPageToElement(selector='.knHJyb')
    action_runner.ClickElement(selector='.knHJyb')


class BackgroundFacebookMobileStory2019(_BackgroundStory):
  NAME = 'background:social:facebook:2019'
  URL = 'https://www.facebook.com/rihanna'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.HEALTH_CHECK, story_tags.YEAR_2019]


class BackgroundNytimesMobileStory2019(browsing_stories.NytimesMobileStory2019):
  NAME = 'background:news:nytimes:2019'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.JAVASCRIPT_HEAVY, story_tags.YEAR_2019]
  ITEMS_TO_VISIT = 1
  ITEM_SCROLL_REPEAT = 1
  ITEM_READ_TIME_IN_SECONDS = 1

  def _Measure(self, action_runner):
    action_runner.tab.browser.Background()
    super(BackgroundNytimesMobileStory2019, self)._Measure(action_runner)


class BackgroundImgurMobileStory2019(_BackgroundStory):
  NAME = 'background:media:imgur:2019'
  URL = 'http://imgur.com/gallery/hUita'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.YEAR_2019]


class BackgroundGmailMobileStory2019(loading_stories.LoadGmailStory2019):
  NAME = 'background:tools:gmail:2019'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.HEALTH_CHECK, story_tags.YEAR_2019]
  EXTRA_BROWSER_ARGUMENTS = []

  def _Measure(self, action_runner):
    action_runner.tab.browser.Background()
    super(BackgroundGmailMobileStory2019, self)._Measure(action_runner)
