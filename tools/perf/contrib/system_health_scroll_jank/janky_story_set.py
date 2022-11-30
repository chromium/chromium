# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets.helpers import override_online
from page_sets.system_health import platforms
from page_sets.system_health import story_tags
from page_sets.system_health import system_health_story
from page_sets.system_health import browsing_stories

from telemetry import story


class JankyStory(system_health_story.SystemHealthStory):
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  SCROLL_STEP = 4000
  SCROLL_SPEED = 1500

  def __init__(self, story_set, take_memory_measurement):
    super(JankyStory, self).__init__(story_set, take_memory_measurement)
    self.script_to_evaluate_on_commit = '''
        window.WebSocket = undefined;
        window.Worker = undefined;
        window.performance = undefined;''' + override_online.ALWAYS_ONLINE

  def _DidLoadDocument(self, action_runner):
    action_runner.WaitForJavaScriptCondition(
        'document.body != null && '
        'document.body.scrollHeight > window.innerHeight && '
        '!document.body.addEventListener("touchstart", function() {})')

    for direction in [1, 1, 1, -1, -1, -1, 1, 1, 1]:
      action_runner.ScrollPage(
          distance=direction * self.SCROLL_STEP,
          use_touch=True,
          speed_in_pixels_per_second=self.SCROLL_SPEED,
      )

  @classmethod
  def GenerateStoryDescription(cls):
    return 'Load %s then make several scrolls.' % cls.URL


# We want to reuse the system_health WPR archive, so the story names should
# be the same as in system_health.common_mobile benchmark.
class FlickrJankyStory(JankyStory):
  NAME = 'browse:media:flickr_infinite_scroll:2019'
  URL = 'https://www.flickr.com/explore'
  TAGS = [story_tags.INFINITE_SCROLL, story_tags.YEAR_2019]


class DiscourseJankyStory(JankyStory):
  NAME = 'browse:tech:discourse_infinite_scroll:2018'
  URL = 'https://meta.discourse.org/t/topic-list-previews/41630/28'
  TAGS = [story_tags.INFINITE_SCROLL, story_tags.YEAR_2019]


class TumblrJankyStory(JankyStory):
  NAME = 'browse:social:tumblr_infinite_scroll:2018'
  URL = 'https://techcrunch.tumblr.com/'
  TAGS = [story_tags.INFINITE_SCROLL, story_tags.YEAR_2019]


class JankyStorySet(story.StorySet):
  def __init__(self):
    super(JankyStorySet, self).__init__(
        archive_data_file='../../page_sets/data/system_health_mobile.json',
        cloud_storage_bucket=story.PARTNER_BUCKET)

    self.AddStory(FlickrJankyStory(self, False))
    self.AddStory(DiscourseJankyStory(self, False))
    self.AddStory(TumblrJankyStory(self, False))
    self.AddStory(browsing_stories.GoogleMapsMobileStory2019(self, False))
    self.AddStory(browsing_stories.CnnStory2021(self, False))
    self.AddStory(browsing_stories.FacebookMobileStory2019(self, False))
    self.AddStory(browsing_stories.TikTokMobileStory2021(self, False))
    self.AddStory(browsing_stories.BusinessInsiderMobile2021(self, False))
    self.AddStory(
        browsing_stories.BusinessInsiderScrollWhileLoadingMobile2021(
            self, False))
    self.AddStory(browsing_stories.YouTubeMobileStory2019(self, False))

  def GetAbridgedStorySetTagFilter(self):
    return story_tags.HEALTH_CHECK.name
