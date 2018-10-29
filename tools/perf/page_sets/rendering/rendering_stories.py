# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from page_sets.rendering import rendering_shared_state as shared_state
from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags
from page_sets.system_health import platforms

from telemetry import story

from py_utils import discover


class RenderingStorySet(story.StorySet):
  """Stories related to rendering."""
  def __init__(self, platform, scroll_forever=False):
    super(RenderingStorySet, self).__init__(
        archive_data_file=('../data/rendering_%s.json' % platform),
        cloud_storage_bucket=story.PARTNER_BUCKET)

    assert platform in platforms.ALL_PLATFORMS

    if platform == platforms.MOBILE:
      shared_page_state_class = shared_state.MobileRenderingSharedState
    elif platform == platforms.DESKTOP:
      shared_page_state_class = shared_state.DesktopRenderingSharedState
    else:
      shared_page_state_class = shared_state.RenderingSharedState

    self.scroll_forever = scroll_forever

    # For pinch zoom page sets, set default to desktop scale factor
    self.target_scale_factor = 4.0

    for story_class in _IterAllRenderingStoryClasses():
      if (story_class.ABSTRACT_STORY or
          platform not in story_class.SUPPORTED_PLATFORMS):
        continue

      required_args = []
      if (story_class.TAGS and
          story_tags.USE_FAKE_CAMERA_DEVICE in story_class.TAGS):
        required_args += [
            # Use a fake camera showing a placeholder video.
            '--use-fake-device-for-media-stream',
            # Don't prompt for camera access. (Conveniently,
            # this takes precedent over --deny-permission-prompts.)
            '--use-fake-ui-for-media-stream',
        ]

      self.AddStory(story_class(
          page_set=self,
          shared_page_state_class=shared_page_state_class,
          extra_browser_args=required_args))

      if (platform == platforms.MOBILE and
          story_class.TAGS and
          story_tags.GPU_RASTERIZATION in story_class.TAGS):
        self.AddStory(story_class(
            page_set=self,
            shared_page_state_class=shared_page_state_class,
            name_suffix='_desktop_gpu_raster',
            extra_browser_args=required_args + [
                '--force-gpu-rasterization',
            ]))

      if (platform == platforms.MOBILE and
          story_class.TAGS and
          story_tags.IMAGE_DECODING in story_class.TAGS):
        self.AddStory(story_class(
            page_set=self,
            shared_page_state_class=shared_page_state_class,
            name_suffix='_gpu_rasterization_and_decoding',
            extra_browser_args=required_args + [
                '--force-gpu-rasterization',
                '--enable-accelerated-jpeg-decoding',
            ]))


class DesktopRenderingStorySet(RenderingStorySet):
  """Desktop stories related to rendering.

  Note: This story set is only intended to be used for recording stories via
  tools/perf/record_wpr. If you would like to use it in a benchmark, please use
  the generic RenderingStorySet class instead (you'll need to override the
  CreateStorySet method of your benchmark).
  """
  def __init__(self):
    super(DesktopRenderingStorySet, self).__init__(platform='desktop')


def _IterAllRenderingStoryClasses():
  start_dir = os.path.dirname(os.path.abspath(__file__))
  # Sort the classes by their names so that their order is stable and
  # deterministic.
  for _, cls in sorted(discover.DiscoverClasses(
      start_dir=start_dir,
      top_level_dir=os.path.dirname(start_dir),
      base_class=rendering_story.RenderingStory).iteritems()):
    yield cls
