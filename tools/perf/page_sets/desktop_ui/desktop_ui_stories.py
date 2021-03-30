# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import story
from page_sets.desktop_ui import download_shelf_story, tab_search_story


class DesktopUIStorySet(story.StorySet):
  TAB_SEARCH_STORIES = [
      tab_search_story.TabSearchStoryTop10,
      tab_search_story.TabSearchStoryTop50,
      tab_search_story.TabSearchStoryTop100,
      tab_search_story.TabSearchStoryTop10Loading,
      tab_search_story.TabSearchStoryTop50Loading,
      tab_search_story.TabSearchStoryTop100Loading,
      tab_search_story.TabSearchStoryCloseAndOpen,
      tab_search_story.TabSearchStoryCloseAndOpenLoading,
      tab_search_story.TabSearchStoryScrollUpAndDown,
      tab_search_story.TabSearchStoryCleanSlate,
      tab_search_story.TabSearchStoryMeasureMemoryBefore,
      tab_search_story.TabSearchStoryMeasureMemoryAfter,
      tab_search_story.TabSearchStoryMeasureMemoryMultiwindow,
      tab_search_story.TabSearchStoryMeasureMemory2TabSearch,
      tab_search_story.TabSearchStoryMeasureMemory3TabSearch,
  ]

  DOWNLOAD_SHELF_STORIES = [
      download_shelf_story.DownloadShelfStory1File,
      download_shelf_story.DownloadShelfStory5File,
  ]

  def __init__(self):
    super(DesktopUIStorySet,
          self).__init__(archive_data_file=('../data/desktop_ui.json'),
                         cloud_storage_bucket=story.PARTNER_BUCKET)
    for cls in self.TAB_SEARCH_STORIES:
      self.AddStory(
          cls(self,
              ['--enable-features=TabSearch', '--top-chrome-touch-ui=disabled'
               ]))

    for cls in self.DOWNLOAD_SHELF_STORIES:
      self.AddStory(cls(self))
