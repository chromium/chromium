# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import story
from page_sets.tab_search import tab_search_story


class TabSearchStorySet(story.StorySet):
  STORIES = [
      tab_search_story.TabSearchStoryTop10,
      tab_search_story.TabSearchStoryTop50,
      tab_search_story.TabSearchStoryTop100,
      tab_search_story.TabSearchStoryTop10Loading,
      tab_search_story.TabSearchStoryTop50Loading,
      tab_search_story.TabSearchStoryTop100Loading,
      tab_search_story.TabSearchStoryCloseAndOpen,
      tab_search_story.TabSearchStoryCloseAndOpenLoading,
      tab_search_story.TabSearchStoryScrollUpAndDown,
  ]

  def __init__(self):
    super(TabSearchStorySet,
          self).__init__(archive_data_file=('../data/tab_search_desktop.json'),
                         cloud_storage_bucket=story.PARTNER_BUCKET)
    for cls in self.STORIES:
      self.AddStory(
          cls(self,
              ['--enable-features=TabSearch', '--top-chrome-touch-ui=disabled'
               ]))
