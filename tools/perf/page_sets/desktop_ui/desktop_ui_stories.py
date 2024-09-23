# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import story
from page_sets.desktop_ui import \
    new_tab_page_story, omnibox_story, \
    tab_search_story, webui_tab_strip_story
from page_sets.desktop_ui.ui_devtools_utils import IsMac


class DesktopUIStorySet(story.StorySet):
  TAB_SEARCH_STORIES = [
      tab_search_story.TabSearchStoryTop10,
      tab_search_story.TabSearchStoryTop50,
      tab_search_story.TabSearchStoryTop10Loading,
      tab_search_story.TabSearchStoryRecentlyClosed10,
      tab_search_story.TabSearchStoryRecentlyClosed50,
      tab_search_story.TabSearchStoryRecentlyClosed100,
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

  WEBUI_TAB_STRIP_STORIES = [
      webui_tab_strip_story.WebUITabStripStoryCleanSlate,
      webui_tab_strip_story.WebUITabStripStoryMeasureMemory,
      webui_tab_strip_story.WebUITabStripStoryMeasureMemory2Window,
      webui_tab_strip_story.WebUITabStripStoryTop10,
      webui_tab_strip_story.WebUITabStripStoryTop10Loading,
  ]

  OMNIBOX_STORIES = [
      omnibox_story.OmniboxStoryPedal,
      omnibox_story.OmniboxStoryScopedSearch,
      omnibox_story.OmniboxStorySearch,
  ]

  NEW_TAB_PAGE_STORIES = [
      new_tab_page_story.NewTabPageStoryLoading,
  ]

  def __init__(self, exhaustive=False):
    super(DesktopUIStorySet,
          self).__init__(archive_data_file=('../data/desktop_ui.json'),
                         cloud_storage_bucket=story.PARTNER_BUCKET)
    for cls in self.TAB_SEARCH_STORIES:
      self.AddStory(
          cls(self, [
              '--top-chrome-touch-ui=disabled',
              '--enable-features=TabSearchUseMetricsReporter',
          ]))

    # WebUI Tab Strip is not available on Mac.
    if not IsMac() or exhaustive:
      for cls in self.WEBUI_TAB_STRIP_STORIES:
        self.AddStory(
            cls(self, [
                '--enable-features=WebUITabStrip',
                '--top-chrome-touch-ui=enabled',
            ]))

    for cls in self.OMNIBOX_STORIES:
      self.AddStory(cls(self))

    for cls in self.NEW_TAB_PAGE_STORIES:
      features = [
          'NtpRecipeTasksModule:NtpRecipeTasksModuleDataParam/fake',
          'NtpChromeCartModule:NtpChromeCartModuleDataParam/fake',
          'NtpDriveModule:NtpDriveModuleDataParam/fake',
          'NtpPhotosModule:NtpPhotosModuleDataParam/1',
      ]
      self.AddStory(
          cls(self, [
              '--enable-features=%s' % ','.join(features),
              '--signed-out-ntp-modules',
          ]))
