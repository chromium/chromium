# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from telemetry import decorators
from telemetry.testing import story_set_smoke_test


class StorySetUnitTest(story_set_smoke_test.StorySetSmokeTest):

  def setUp(self):
    self.story_sets_dir = os.path.dirname(os.path.realpath(__file__))
    self.top_level_dir = os.path.dirname(self.story_sets_dir)

  # TODO(tbarzic): crbug.com/386416.
  @decorators.Disabled('chromeos')
  def testSmoke(self):
    self.RunSmokeTest(self.story_sets_dir, self.top_level_dir)

  def testNoStorySetDefinedWithUnnamedStories(self):
    for story_set_class in self.GetAllStorySetClasses(self.story_sets_dir,
                                                      self.top_level_dir):
      story_set = story_set_class()
      for story in story_set:
        self.assertTrue(story.name != '',
                        'stories must be named: ' + str(story_set_class))
