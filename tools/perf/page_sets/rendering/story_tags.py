# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections


Tag = collections.namedtuple('Tag', ['name', 'description'])


# Below are tags that describe various aspect of rendering stories.
# A story can have multiple tags. All the tags should be nouns.

GPU_RASTERIZATION = Tag(
    'gpu_rasterization', 'Story tests performance with GPU rasterization.')
FASTPATH = Tag(
    'fastpath', 'Fast path stories.')
REQUIRED_WEBGL = Tag(
    'required_webgl', 'Stories that are skipped if no webgl support')
USE_FAKE_CAMERA_DEVICE = Tag(
    'use_fake_camera_device', 'Story requires a camera device for media')

# Below are tags for filtering by page sets

BACKDROP_FILTER = Tag(
    'backdrop_filter', 'Backdrop filter stories')
IMAGE_DECODING = Tag(
    'image_decoding', ('Stories decoding JPEG and WebP (and using GPU '
                       'rasterization) to compare YUV and RGB'))
KEY_DESKTOP_MOVE = Tag(
    'key_desktop_move', 'Key desktop move stories')
KEY_HIT_TEST = Tag(
    'key_hit_test', 'Key hit test stories')
KEY_SILK = Tag(
    'key_silk', 'Key silk stories')
KEY_NOOP = Tag(
    'key_noop', 'Key noop stories')
KEY_IDLE_POWER = Tag(
    'key_idle_power', 'Key idle power stories')
MAPS = Tag(
    'maps', 'Maps stories')
MOTIONMARK = Tag(
    'motionmark', 'Motionmark benchmark stories')
PATHOLOGICAL_MOBILE_SITES = Tag(
    'pathological_mobile_sites', 'Pathological mobile sites')
POLYMER = Tag(
    'polymer', 'Polymer stories')
REPAINT_DESKTOP = Tag(
    'repaint_desktop', 'Repaint desktop stories')
# Representative story_tags are the cluster representatives of benchamrks
# Documentation: https://goto.google.com/chrome-benchmark-clustering
REPRESENTATIVE_MAC_DESKTOP = Tag(
    'representative_mac_desktop', 'Rendering desktop representatives for mac')
REPRESENTATIVE_MOBILE = Tag(
    'representative_mobile', 'Rendering mobile representatives')
REPRESENTATIVE_WIN_DESKTOP = Tag(
    'representative_win_desktop',
    'Rendering desktop representatives for windows')
SIMPLE_MOBILE_SITES = Tag(
    'simple_mobile_sites', 'Simple mobile sites')
THROUGHPUT_TEST = Tag(
    'throughput_test', 'Test cases for throughput measurement')
TOP_REAL_WORLD_DESKTOP = Tag(
    'top_real_world_desktop', 'Top real world desktop stories')
TOP_REAL_WORLD_MOBILE = Tag(
    'top_real_world_mobile', 'Top real world mobile stories')
TOUGH_ANIMATION = Tag(
    'tough_animation', 'Tough animation stories')
TOUGH_CANVAS = Tag(
    'tough_canvas', 'Tough canvas stories')
TOUGH_COMPOSITOR = Tag(
    'tough_compositor', 'Tough compositor stories')
TOUGH_FILTERS = Tag(
    'tough_filters', 'Tough filters stories')
TOUGH_IMAGE_DECODE = Tag(
    'tough_image_decode', 'Tough image decode stories')
TOUGH_PATH_RENDERING = Tag(
    'tough_path_rendering', 'Tough path rendering stories')
TOUGH_PINCH_ZOOM = Tag(
    'tough_pinch_zoom', 'Tough pinch zoom stories (only on Mac for desktop)')
TOUGH_PINCH_ZOOM_MOBILE = Tag(
    'tough_pinch_zoom_mobile', 'Tough pinch zoom mobile stories')
TOUGH_SCHEDULING = Tag(
    'tough_scheduling', 'Tough scheduling stories')
TOUGH_SCROLLING = Tag(
    'tough_scrolling', 'Tough scrolling stories')
TOUGH_TEXTURE_UPLOAD = Tag(
    'tough_texture_upload', 'Tough texture upload stories')
TOUGH_WEBGL = Tag(
    'tough_webgl', 'Tough webgl stories')


def _ExtractAllTags():
  all_tag_names = set()
  all_tags = []
  # Collect all the tags defined in this module. Also assert that there is no
  # duplicate tag names.
  for obj in globals().values():
    if isinstance(obj, Tag):
      all_tags.append(obj)
      assert obj.name not in all_tag_names, 'Duplicate tag name: %s' % obj.name
      all_tag_names.add(obj.name)
  return all_tags


ALL_TAGS = _ExtractAllTags()
