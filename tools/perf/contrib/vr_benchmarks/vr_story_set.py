# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from telemetry import story


class VrStorySet(story.StorySet):
  def __init__(self, use_fake_pose_tracker=True, **kwargs):
    self._use_fake_pose_tracker = use_fake_pose_tracker
    super(VrStorySet, self).__init__(**kwargs)

  @property
  def use_fake_pose_tracker(self):
    return self._use_fake_pose_tracker
