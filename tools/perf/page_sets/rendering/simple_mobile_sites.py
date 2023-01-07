# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags
from page_sets.system_health import platforms


class SimplePage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.SIMPLE_MOBILE_SITES]

  def __init__(self,
               page_set,
               shared_page_state_class=(
                   shared_page_state.SharedMobilePageState),
               name_suffix='',
               extra_browser_args=None):
    super(SimplePage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(SimplePage, self).RunNavigateSteps(action_runner)
    # TODO(epenner): Remove this wait (http://crbug.com/366933)
    action_runner.Wait(5)

  def RunPageInteractions(self, action_runner):
    # Make the scroll longer to reduce noise.
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage(direction='down', speed_in_pixels_per_second=300)


class SimpleEbay2018Page(SimplePage):
  BASE_NAME = 'ebay_scroll'
  YEAR = '2018'
  URL = 'http://www.ebay.co.uk/'


class SimpleFlickr2018Page(SimplePage):
  BASE_NAME = 'flickr_scroll'
  YEAR = '2018'
  URL = 'https://www.flickr.com/photos/flickr/albums/72157639858715274'


class SimpleNYCGov2018Page(SimplePage):
  BASE_NAME = 'nyc_gov_scroll'
  YEAR = '2018'
  URL = 'http://www.nyc.gov'


class SimpleNYTimes2018Page(SimplePage):
  BASE_NAME = 'nytimes_scroll'
  YEAR = '2018'
  URL = 'http://m.nytimes.com/'
