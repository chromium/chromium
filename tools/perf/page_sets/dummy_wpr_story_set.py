# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page as page_module
from telemetry import story


class DummyWprPage(page_module.Page):

  def __init__(self, page_set):
    super(DummyWprPage, self).__init__(
      url='https://google.com',
      name='google_main_page',
      page_set=page_set)


class DummyWprStorySet(story.StorySet):

  def __init__(self):
    super(DummyWprStorySet, self).__init__(
      archive_data_file='data/dummy_wpr.json',
      cloud_storage_bucket=story.PARTNER_BUCKET)
    self.AddStory(DummyWprPage(self))
