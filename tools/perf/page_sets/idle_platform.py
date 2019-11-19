# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import time

from telemetry.page import shared_page_state
from telemetry import story


class _IdleSharedState(shared_page_state.SharedPageState):
  def __init__(self, test, finder_options, story_set, possible_browser=None):
    super(_IdleSharedState, self).__init__(
        test, finder_options, story_set, possible_browser)
    self._current_story = None

  def WillRunStory(self, current_story):
    self._current_story = current_story
    assert self.platform.tracing_controller.is_tracing_running

  def RunStory(self, _):
    self._current_story.Run(self)

  def DidRunStory(self, _):
    self._current_story = None


class _IdleStory(story.Story):
  def __init__(self, name, duration):
    super(_IdleStory, self).__init__(
        shared_state_class=_IdleSharedState, name=name)
    self._duration = duration
    # https://github.com/catapult-project/catapult/issues/3489
    # Even though there is no actual url being used, it is required for
    # uploading results using the --upload-results flag. Remove url when
    # it is no longer needed.
    self._url = name

  def Run(self, shared_state):
    time.sleep(self._duration)

  @property
  def url(self):
    return self._url


class IdleStorySet(story.StorySet):
  def __init__(self):
    super(IdleStorySet, self).__init__()
    self.AddStory(_IdleStory('IdleStory_10s', 10))
    self.AddStory(_IdleStory('IdleStory_60s', 60))
    self.AddStory(_IdleStory('IdleStory_120s', 120))
