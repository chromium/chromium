# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from page_sets.system_health import chrome_stories
from page_sets.system_health import platforms
from page_sets.system_health import story_tags
from page_sets.system_health import system_health_story

from telemetry import story

from py_utils import discover


class SystemHealthStorySet(story.StorySet):
  """User stories for the System Health Plan.

  See https://goo.gl/Jek2NL.
  """

  def __init__(self,
               platform,
               case=None,
               take_memory_measurement=False,
               tag=None):
    super(SystemHealthStorySet, self).__init__(
        archive_data_file=('../data/system_health_%s.json' % platform),
        cloud_storage_bucket=story.PARTNER_BUCKET)

    assert platform in platforms.ALL_PLATFORMS

    def IncludeStory(story_class):
      if story_class.ABSTRACT_STORY:
        return False
      if platform not in story_class.SUPPORTED_PLATFORMS:
        return False
      if case and not story_class.NAME.startswith(case + ':'):
        return False
      if tag and not tag in story_class.TAGS:
        return False
      return True

    for story_class in IterAllSystemHealthStoryClasses():
      if IncludeStory(story_class):
        if platform == 'mobile':
          # Extra browser args are disabled in the mobile platform
          story_class.EXTRA_BROWSER_ARGUMENTS = []
        self.AddStory(story_class(self, take_memory_measurement))

  def GetAbridgedStorySetTagFilter(self):
    return story_tags.HEALTH_CHECK.name


class SystemHealthBlankStorySet(story.StorySet):
  """A story set containing the chrome:blank story only."""
  def __init__(self, take_memory_measurement=False):
    super(SystemHealthBlankStorySet, self).__init__()
    self.AddStory(
        chrome_stories.BlankAboutBlankStory(self, take_memory_measurement))


class DesktopSystemHealthStorySet(SystemHealthStorySet):
  """Desktop user stories for the System Health Plan.

  Note: This story set is only intended to be used for recording stories via
  tools/perf/record_wpr. If you would like to use it in a benchmark, please use
  the generic SystemHealthStorySet class instead (you'll need to override the
  CreateStorySet method of your benchmark).
  """
  def __init__(self):
    super(DesktopSystemHealthStorySet, self).__init__(
        'desktop', take_memory_measurement=False)


class MobileSystemHealthStorySet(SystemHealthStorySet):
  """Mobile user stories for the System Health Plan.

  Note: This story set is only intended to be used for recording stories via
  tools/perf/record_wpr. If you would like to use it in a benchmark, please use
  the generic SystemHealthStorySet class instead (you'll need to override the
  CreateStorySet method of your benchmark).
  """
  def __init__(self):
    super(MobileSystemHealthStorySet, self).__init__(
        'mobile', take_memory_measurement=False)


def IterAllSystemHealthStoryClasses():
  """Generator for system health stories.

  Yields:
    All appropriate SystemHealthStory subclasses defining stories.
  """
  start_dir = os.path.dirname(os.path.abspath(__file__))
  # Sort the classes by their names so that their order is stable and
  # deterministic.
  for unused_cls_name, cls in sorted(discover.DiscoverClasses(
      start_dir=start_dir,
      top_level_dir=os.path.dirname(start_dir),
      base_class=system_health_story.SystemHealthStory).items()):
    yield cls
