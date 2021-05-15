# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
from telemetry import story
from page_sets.desktop_ui import download_shelf_story, tab_search_story, webui_tab_strip_story


class DesktopUIStorySet(story.StorySet):
  TAB_SEARCH_STORIES = [
      tab_search_story.TabSearchStoryTop10,
      tab_search_story.TabSearchStoryTop50,
      tab_search_story.TabSearchStoryTop10Loading,
      tab_search_story.TabSearchStoryTop50Loading,
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
      download_shelf_story.DownloadShelfStoryMeasureMemory,
      download_shelf_story.DownloadShelfStoryTop10Loading,
  ]

  DOWNLOAD_SHELF_WEBUI_STORIES = [
      download_shelf_story.DownloadShelfWebUIStory1File,
      download_shelf_story.DownloadShelfWebUIStory5File,
      download_shelf_story.DownloadShelfWebUIStoryMeasureMemory,
      download_shelf_story.DownloadShelfWebUIStoryTop10Loading,
  ]

  WEBUI_TAB_STRIP_STORIES = [
      webui_tab_strip_story.WebUITabStripStoryCleanSlate,
      webui_tab_strip_story.WebUITabStripStoryTop10,
      webui_tab_strip_story.WebUITabStripStoryTop10Loading,
  ]

  def __init__(self):
    super(DesktopUIStorySet,
          self).__init__(archive_data_file=('../data/desktop_ui.json'),
                         cloud_storage_bucket=story.PARTNER_BUCKET)
    for cls in self.TAB_SEARCH_STORIES:
      self.AddStory(
          cls(self, [
              '--enable-ui-devtools=enabled',
              '--top-chrome-touch-ui=disabled',
          ]))

    for cls in self.DOWNLOAD_SHELF_STORIES:
      self.AddStory(cls(self, [
          '--enable-ui-devtools=enabled',
      ]))

    for cls in self.DOWNLOAD_SHELF_WEBUI_STORIES:
      self.AddStory(
          cls(self, [
              '--enable-features=WebUIDownloadShelf',
              '--enable-ui-devtools=enabled',
          ]))

    # WebUI Tab Strip is not available on Mac.
    if sys.platform != 'darwin':
      for cls in self.WEBUI_TAB_STRIP_STORIES:
        self.AddStory(
            cls(self, [
                '--enable-features=WebUITabStrip',
                '--enable-ui-devtools=enabled',
                '--top-chrome-touch-ui=enabled',
            ]))
