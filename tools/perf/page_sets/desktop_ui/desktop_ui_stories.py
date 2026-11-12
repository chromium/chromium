# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import story
from page_sets.desktop_ui import (
  new_tab_page_story,
  omnibox_story,
  organizer_panel_story,
  tab_search_story,
)


class DesktopUIStorySet(story.StorySet):
  # TODO(crbug.com/585173863): Delete tab search stories once organizer panel is
  # launched.
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

  ORGANIZER_PANEL_STORIES = [
    organizer_panel_story.OrganizerPanelStoryTop10,
    organizer_panel_story.OrganizerPanelStoryTop50,
    organizer_panel_story.OrganizerPanelStoryTop10Loading,
    organizer_panel_story.OrganizerPanelStoryRecentlyClosed10,
    organizer_panel_story.OrganizerPanelStoryRecentlyClosed50,
    organizer_panel_story.OrganizerPanelStoryRecentlyClosed100,
    organizer_panel_story.OrganizerPanelStoryTop50Loading,
    organizer_panel_story.OrganizerPanelStoryCloseAndOpen,
    organizer_panel_story.OrganizerPanelStoryCloseAndOpenLoading,
    organizer_panel_story.OrganizerPanelStoryScrollUpAndDown,
    organizer_panel_story.OrganizerPanelStoryCleanSlate,
    organizer_panel_story.OrganizerPanelStoryMeasureMemoryBefore,
    organizer_panel_story.OrganizerPanelStoryMeasureMemoryAfter,
    organizer_panel_story.OrganizerPanelStoryMeasureMemoryMultiwindow,
    organizer_panel_story.OrganizerPanelStoryMeasureMemory2OrganizerPanel,
    organizer_panel_story.OrganizerPanelStoryMeasureMemory3OrganizerPanel,
  ]

  OMNIBOX_STORIES = [
    omnibox_story.OmniboxStoryPedal,
    omnibox_story.OmniboxStoryScopedSearch,
    omnibox_story.OmniboxStorySearch,
  ]

  NEW_TAB_PAGE_STORIES = [
    new_tab_page_story.NewTabPageStoryLoading,
  ]

  def __init__(self):
    super(DesktopUIStorySet, self).__init__(
      archive_data_file=('../data/desktop_ui.json'),
      cloud_storage_bucket=story.PARTNER_BUCKET,
    )
    # TODO(crbug.com/585173863): Delete tab search stories once organizer panel
    # is launched.
    for cls in self.TAB_SEARCH_STORIES:
      self.AddStory(
        cls(
          self,
          [
            '--top-chrome-touch-ui=disabled',
            '--enable-features=TabSearchUseMetricsReporter',
            '--disable-features=OrganizerPanel',
          ],
        )
      )

    for cls in self.ORGANIZER_PANEL_STORIES:
      self.AddStory(
        cls(
          self,
          [
            '--top-chrome-touch-ui=disabled',
            '--enable-features=OrganizerPanel',
          ],
        )
      )

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
        cls(
          self,
          [
            '--enable-features=%s' % ','.join(features),
            '--signed-out-ntp-modules',
          ],
        )
      )
