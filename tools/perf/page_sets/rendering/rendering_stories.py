# Copyright 2018 The Chromium Authors
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

  def __init__(self, platform, scroll_forever=False, disable_tracing=False):
    super(RenderingStorySet, self).__init__(
        archive_data_file=('../data/rendering_%s.json' % platform),
        cloud_storage_bucket=story.PARTNER_BUCKET)

    assert platform in platforms.ALL_PLATFORMS
    self._platform = platform

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
      if (story_class.ABSTRACT_STORY
          or platform not in story_class.SUPPORTED_PLATFORMS
          or story_class.DISABLE_TRACING != disable_tracing):
        continue

      required_args = []
      name_suffix = ''
      if (story_class.TAGS and
          story_tags.USE_FAKE_CAMERA_DEVICE in story_class.TAGS):
        required_args += [
            # Use a fake camera showing a placeholder video.
            '--use-fake-device-for-media-stream',
            # Don't prompt for camera access. (Conveniently,
            # this takes precedent over --deny-permission-prompts.)
            '--use-fake-ui-for-media-stream',
        ]

      if story_class.TAGS and story_tags.BACKDROP_FILTER in story_class.TAGS:
        # Experimental web platform features must be enabled in order for the
        # 'backdrop-filter' CSS property to work.
        required_args.append('--enable-experimental-web-platform-features')

      # TODO(crbug.com/968125): We must run without out-of-process rasterization
      # until that branch is implemented for YUV decoding.
      if (story_class.TAGS and
          story_tags.IMAGE_DECODING in story_class.TAGS and
          story_tags.GPU_RASTERIZATION in story_class.TAGS):
        required_args += ['--enable-gpu-rasterization']
        # Run RGB decoding with GPU rasterization (to be most comparable to YUV)
        self.AddStory(story_class(
            page_set=self,
            extra_browser_args=required_args +
                ['--disable-yuv-image-decoding'],
            shared_page_state_class=shared_page_state_class,
            name_suffix='_rgb_and_gpu_rasterization'))
        # Also run YUV decoding story with GPU rasterization.
        name_suffix = '_yuv_and_gpu_rasterization'

      self.AddStory(story_class(
          page_set=self,
          extra_browser_args=required_args,
          shared_page_state_class=shared_page_state_class,
          name_suffix=name_suffix))

  def GetAbridgedStorySetTagFilter(self):
    if self._platform == platforms.DESKTOP:
      if os.name == 'nt':
        return 'representative_win_desktop'
      # There is no specific tag for linux, cros, etc,
      # so just use mac's.
      return 'representative_mac_desktop'
    if self._platform == platforms.MOBILE:
      return 'representative_mobile'
    raise RuntimeError('Platform {} is not in the list of expected platforms.')


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
      base_class=rendering_story.RenderingStory).items()):
    yield cls
