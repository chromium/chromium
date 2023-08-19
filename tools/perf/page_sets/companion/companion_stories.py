# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import story
from page_sets.companion import basic_companion_story


class CompanionStorySet(story.StorySet):
  COMPANION_STORIES = [
      basic_companion_story.CompanionStoryBasicOpen,
      basic_companion_story.CompanionStoryBasicOpenLoggedOut,
      basic_companion_story.CompanionStorySRP,
      basic_companion_story.CompanionStoryScreenshot
  ]

  def __init__(self):
    super(CompanionStorySet,
          self).__init__(cloud_storage_bucket=story.PARTNER_BUCKET)

    for cls in self.COMPANION_STORIES:
      self.AddStory(cls(self))
