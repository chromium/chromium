# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import six

from page_sets.system_health import platforms
from page_sets.system_health import story_tags

from telemetry.page import page as page_module
from telemetry.page import shared_page_state
from telemetry.util import wpr_modes


# Extra wait time after the page has loaded required by the loading metric. We
# use it in all benchmarks to avoid divergence between benchmarks.
# TODO(petrcermak): Switch the memory benchmarks to use it as well.
_WAIT_TIME_AFTER_LOAD = 10


class _SystemHealthSharedState(shared_page_state.SharedPageState):
  """Shared state which enables disabling stories on individual platforms.
     This should be used only to disable the stories permanently. For
     disabling stories temporarily use story expectations in ./expectations.py.
  """

  def CanRunOnBrowser(self, browser_info, page):
    if (browser_info.browser_type.startswith('android-webview')
        and page.WEBVIEW_NOT_SUPPORTED):
      return False

    if page.TAGS and story_tags.WEBGL in page.TAGS:
      return browser_info.HasWebGLSupport()
    return True


class _MetaSystemHealthStory(type):
  """Metaclass for SystemHealthStory."""

  @property
  def ABSTRACT_STORY(cls):
    """Class field marking whether the class is abstract.

    If true, the story will NOT be instantiated and added to a System Health
    story set. This field is NOT inherited by subclasses (that's why it's
    defined on the metaclass).
    """
    return cls.__dict__.get('ABSTRACT_STORY', False)


class SystemHealthStory(
    six.with_metaclass(_MetaSystemHealthStory, page_module.Page)):
  """Abstract base class for System Health user stories."""

  # The full name of a single page story has the form CASE:GROUP:PAGE:[VERSION]
  # (e.g. 'load:search:google' or 'load:search:google:2018').
  NAME = NotImplemented
  URL = NotImplemented
  ABSTRACT_STORY = True
  # Skip the login flow in replay mode
  # If you want to replay the login flow in your story, set SKIP_LOGIN to False
  SKIP_LOGIN = True
  SUPPORTED_PLATFORMS = platforms.ALL_PLATFORMS
  TAGS = []
  PLATFORM_SPECIFIC = False
  WEBVIEW_NOT_SUPPORTED = False

  def __init__(self, story_set, take_memory_measurement,
      extra_browser_args=None):
    case, group, _ = self.NAME.split(':', 2)
    tags = []
    found_year_tag = False
    for t in self.TAGS:  # pylint: disable=not-an-iterable
      assert t in story_tags.ALL_TAGS
      tags.append(t.name)
      if t in story_tags.YEAR_TAGS:
        # Assert that this is the first year tag.
        assert not found_year_tag, (
            "%s has more than one year tag found." % self.__class__.__name__)
        found_year_tag = True
    # Assert that there is one year tag.
    assert found_year_tag, (
        "%s needs exactly one year tag." % self.__class__.__name__)
    super(SystemHealthStory, self).__init__(
        shared_page_state_class=_SystemHealthSharedState,
        page_set=story_set, name=self.NAME, url=self.URL, tags=tags,
        grouping_keys={'case': case, 'group': group},
        platform_specific=self.PLATFORM_SPECIFIC,
        extra_browser_args=extra_browser_args)
    self._take_memory_measurement = take_memory_measurement

  @classmethod
  def GetStoryDescription(cls):
    if cls.__doc__:
      return cls.__doc__
    return cls.GenerateStoryDescription()

  @classmethod
  def GenerateStoryDescription(cls):
    """ Subclasses of SystemHealthStory can override this to auto generate
    their story description.
    However, it's recommended to use the Python docstring to describe the user
    stories instead and this should only be used for very repetitive cases.
    """
    return None

  def _Measure(self, action_runner):
    if self._take_memory_measurement:
      action_runner.MeasureMemory(deterministic_mode=True)
    else:
      action_runner.Wait(_WAIT_TIME_AFTER_LOAD)

  def _Login(self, action_runner):
    pass

  def _DidLoadDocument(self, action_runner):
    pass

  def RunNavigateSteps(self, action_runner):
    if not (self.SKIP_LOGIN and self.wpr_mode == wpr_modes.WPR_REPLAY):
      self._Login(action_runner)
    super(SystemHealthStory, self).RunNavigateSteps(action_runner)

  def RunPageInteractions(self, action_runner):
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()
    self._DidLoadDocument(action_runner)
    self._Measure(action_runner)
