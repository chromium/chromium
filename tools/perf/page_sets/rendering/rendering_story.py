# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import six

from page_sets.rendering import story_tags
from page_sets.system_health import platforms

from telemetry.page import page


class _MetaRenderingStory(type):
  """Metaclass for RenderingStory."""

  @property
  def ABSTRACT_STORY(cls):
    """Class field marking whether the class is abstract.

    If true, the page will NOT be instantiated and added to a Rendering
    page set. This field is NOT inherited by subclasses (that's why it's
    defined on the metaclass).
    """
    return cls.__dict__.get('ABSTRACT_STORY', False)


class RenderingStory(six.with_metaclass(_MetaRenderingStory, page.Page)):
  """Abstract base class for Rendering user stories."""

  BASE_NAME = NotImplemented
  URL = NotImplemented
  ABSTRACT_STORY = True
  SUPPORTED_PLATFORMS = platforms.ALL_PLATFORMS
  TAGS =[]
  PLATFORM_SPECIFIC = False
  YEAR = None
  DISABLE_TRACING = False
  EXTRA_BROWSER_ARGUMENTS = None

  def __init__(self,
               page_set,
               shared_page_state_class,
               name_suffix='',
               extra_browser_args=None,
               make_javascript_deterministic=True,
               base_dir=None,
               perform_final_navigation=True):
    tags = []
    for t in self.TAGS:
      assert t in story_tags.ALL_TAGS
      tags.append(t.name)
    name = self.BASE_NAME + name_suffix
    if self.YEAR:
      name += ('_' + self.YEAR)
    super(RenderingStory, self).__init__(
        page_set=page_set,
        name=name,
        url=self.URL,
        tags=tags,
        platform_specific=self.PLATFORM_SPECIFIC,
        shared_page_state_class=shared_page_state_class,
        extra_browser_args=extra_browser_args,
        make_javascript_deterministic=make_javascript_deterministic,
        base_dir=base_dir,
        perform_final_navigation=perform_final_navigation)

  def WillStartTracing(self, chrome_trace_config):
    chrome_trace_config.category_filter.AddIncludedCategory('benchmark')
    chrome_trace_config.category_filter.AddIncludedCategory('v8')
