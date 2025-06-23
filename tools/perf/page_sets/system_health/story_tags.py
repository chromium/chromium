# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import re


Tag = collections.namedtuple('Tag', ['name', 'description'])


# Below are tags that describe various aspect of system health stories.
# A story can have multiple tags. All the tags should be noun.

ACCESSIBILITY = Tag(
    'accessibility', 'Story tests performance when accessibility is enabled.')
AUDIO_PLAYBACK = Tag(
    'audio_playback', 'Story has audio playing.')
CANVAS_ANIMATION = Tag(
    'canvas_animation', 'Story has animations that are implemented using '
    'html5 canvas.')
CSS_ANIMATION = Tag(
    'css_animation', 'Story has animations that are implemented using CSS.')
EXTENSION = Tag(
    'extension', 'Story has browser with extension installed.')
HEALTH_CHECK = Tag(  # See go/health_check_stories for details.
    'health_check', 'Story belongs to a collection chosen to have a wide '
    'coverage but low running time.')
IMAGES = Tag(
    'images', 'Story has sites with heavy uses of images.')
INFINITE_SCROLL = Tag('infinite_scroll', 'Story has infinite scroll action.')
INTERNATIONAL = Tag(
    'international', 'Story has navigations to websites with content in non '
    'English languages.')
EMERGING_MARKET = Tag(
    'emerging_market', 'Story has significant usage in emerging markets with '
    'low-end mobile devices and slow network connections.')
JAVASCRIPT_HEAVY = Tag(
    'javascript_heavy', 'Story has navigations to websites with heavy usages '
    'of JavaScript. The story uses 20Mb+ memory for javascript and local '
    'run with "v8" category enabled also shows the trace has js slices across '
    'the whole run.')
KEYBOARD_INPUT = Tag(
    'keyboard_input', 'Story does keyboard input.')
SCROLL = Tag(
    'scroll', 'Story has scroll gestures & scroll animation.')
PINCH_ZOOM = Tag(
    'pinch_zoom', 'Story has pinch zoom gestures & pinch zoom animation.')
TABS_SWITCHING = Tag(
    'tabs_switching', 'Story has multi tabs and tabs switching action.')
VIDEO_PLAYBACK = Tag(
    'video_playback', 'Story has video playing.')
WEBASSEMBLY = Tag('wasm', 'Story with heavy usages of WebAssembly')
WEBGL = Tag(
    'webgl', 'Story has sites with heavy uses of WebGL.')
WEB_STORAGE = Tag(
    'web_storage', 'Story has sites with heavy uses of Web storage.')

# Tags by year.
YEAR_2016 = Tag('2016', 'Story was created or updated in 2016.')
YEAR_2017 = Tag('2017', 'Story was created or updated in 2017.')
YEAR_2018 = Tag('2018', 'Story was created or updated in 2018.')
YEAR_2019 = Tag('2019', 'Story was created or updated in 2019.')
YEAR_2020 = Tag('2020', 'Story was created or updated in 2020.')
YEAR_2021 = Tag('2021', 'Story was created or updated in 2021.')
YEAR_2023 = Tag('2023', 'Story was created or updated in 2023.')


def _ExtractAllTags():
  all_tag_names = set()
  all_tags = set()
  # Collect all the tags defined in this module. Also assert that there is no
  # duplicate tag names.
  for obj in globals().values():
    if isinstance(obj, Tag):
      all_tags.add(obj)
      assert obj.name not in all_tag_names, 'Duplicate tag name: %s' % obj.name
      all_tag_names.add(obj.name)
  return all_tags

def _ExtractYearTags():
  year_tags = set()
  pattern = re.compile('^[0-9]{4}$')
  for obj in globals().values():
    if isinstance(obj, Tag) and pattern.match(obj.name):
      year_tags.add(obj)
  return year_tags

ALL_TAGS = _ExtractAllTags()
YEAR_TAGS = _ExtractYearTags()
