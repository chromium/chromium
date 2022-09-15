# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page
from telemetry.page import shared_page_state
from telemetry import story


class WebWorkerStory(page.Page):
  """This story creates 1000 web workers one after another."""
  NAME = 'WebWorker'
  URL = 'file://web_workers/index.html?auto=1&workers=1000'

  def __init__(self, page_set, shared_page_state_class, measure_memory):
    super(WebWorkerStory, self).__init__(
        url=self.URL, page_set=page_set, name=self.NAME,
        shared_page_state_class=shared_page_state_class)
    self.measure_memory = measure_memory

  def RunPageInteractions(self, action_runner):
    action_runner.WaitForJavaScriptCondition('done', timeout=100)
    if self.measure_memory:
      action_runner.MeasureMemory(deterministic_mode=True)


class WebWorkerStorySet(story.StorySet):
  def __init__(self, shared_state = shared_page_state.SharedPageState,
               measure_memory=False):
    super(WebWorkerStorySet, self).__init__(
        cloud_storage_bucket=story.PUBLIC_BUCKET)
    self.AddStory(WebWorkerStory(self, shared_state, measure_memory))
