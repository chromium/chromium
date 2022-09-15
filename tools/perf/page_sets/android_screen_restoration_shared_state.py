# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import shared_page_state


class AndroidScreenRestorationSharedState(shared_page_state.SharedPageState):
  """ Ensures the screen is on before and after each user story is run. """

  def WillRunStory(self, story):
    super(AndroidScreenRestorationSharedState, self).WillRunStory(story)
    self._EnsureScreenOn()

  def DidRunStory(self, results):
    try:
      super(AndroidScreenRestorationSharedState, self).DidRunStory(results)
    finally:
      self._EnsureScreenOn()

  def _EnsureScreenOn(self):
    self.platform.android_action_runner.TurnScreenOn()
