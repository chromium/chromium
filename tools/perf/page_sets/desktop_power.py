# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page as page_module
from telemetry.page import shared_page_state
from telemetry import story
from page_sets import trivial_sites


class _SitIdlePage(page_module.Page):
  def __init__(self, page_set, url, name):
    super(_SitIdlePage, self).__init__(url=url, page_set=page_set, name=name)

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(30)


class DesktopPowerStorySet(story.StorySet):
  def __init__(self, shared_state = shared_page_state.SharedPageState):
    super(DesktopPowerStorySet, self).__init__(
        archive_data_file='data/desktop_power_stories.json',
        cloud_storage_bucket=story.PARTNER_BUCKET)

    # Trivial static sites that shouldn't use much power.
    self.AddStory(trivial_sites.TrivialScrollingPage(
        self, shared_state, wait_in_seconds=30, measure_memory=False))
    self.AddStory(trivial_sites.TrivialBlinkingCursorPage(
        self, shared_state, wait_in_seconds=30, measure_memory=False))
    self.AddStory(trivial_sites.TrivialCanvasPage(
        self, shared_state, wait_in_seconds=30, measure_memory=False))
    self.AddStory(trivial_sites.TrivialWebGLPage(
        self, shared_state, wait_in_seconds=30, measure_memory=False))
    self.AddStory(trivial_sites.TrivialBlurAnimationPage(
        self, shared_state, wait_in_seconds=30, measure_memory=False))
    self.AddStory(trivial_sites.TrivialFullscreenVideoPage(
        self, shared_state, wait_in_seconds=30, measure_memory=False))
    self.AddStory(trivial_sites.TrivialGifPage(
        self, shared_state, wait_in_seconds=30, measure_memory=False))
    self.AddStory(trivial_sites.TrivialAnimationPage(
        self, shared_state, wait_in_seconds=30, measure_memory=False))

    # Sites that have used too much power in the past.
    # http://crbug.com/505990
    self.AddStory(_SitIdlePage(self, 'http://abcnews.go.com/', 'abcnews'))
    # http://crbug.com/505601
    self.AddStory(_SitIdlePage(
        self, 'http://www.slideshare.net/patrickmeenan', 'slideshare'))
    # http://crbug.com/505553
    self.AddStory(_SitIdlePage(self, 'https://instagram.com/cnn/', 'instagram'))
    # http://crbug.com/505544
    self.AddStory(_SitIdlePage(self, 'http://www.sina.com.cn', 'sina'))
    # http://crbug.com/505054
    self.AddStory(_SitIdlePage(self, 'http://www.uol.com.br', 'uol'))
    # http://crbug.com/505052
    self.AddStory(_SitIdlePage(self, 'http://www.indiatimes.com', 'indiatimes'))
    # http://crbug.com/505002
    self.AddStory(_SitIdlePage(self, 'http://www.microsoft.com', 'microsoft'))
