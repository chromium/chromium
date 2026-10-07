# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Same-origin navigations with and without cross-document view transitions."""

import random

from telemetry import story
from telemetry.page import page as page_module

# Number of measured navigations per story run.
_NAVIGATIONS = 20

# Number of navigations per story run in benchmark_smoke_unittest.py, which
# runs the first story on the CQ to check that the benchmark works.
_SMOKE_TEST_NAVIGATIONS = 2

# Upper bound of a uniform random delay that is added to the time between
# navigations, so that navigations don't start at a fixed vsync phase.
_JITTER_SECONDS = 0.05


class _NavigationPage(page_module.Page):
  """Navigates between two same-origin pages and records their FCP.

  The navigations are renderer-initiated, like following a link, so there is
  a cross-document view transition if the pages opt into it.
  """

  def __init__(
    self, page_set, name, tags, view_transition, delay_seconds, navigations
  ):
    super().__init__(
      url='file://view_transitions/navigation.html?vt=%d&i=0'
      % int(view_transition),
      page_set=page_set,
      name=name,
      tags=tags,
      make_javascript_deterministic=False,
    )
    self.view_transition = view_transition
    self._delay_seconds = delay_seconds
    self._navigations = navigations
    self.fcp_samples = []
    self.view_transition_mismatches = 0

  def RunPageInteractions(self, action_runner):
    self.fcp_samples = []
    self.view_transition_mismatches = 0
    # A fixed seed gives every story run the same sequence of delays, so that
    # the vt_* and no_vt_* stories are comparable.
    rng = random.Random(0)
    action_runner.WaitForJavaScriptCondition(
      'window.__results !== undefined && window.__results.fcp !== undefined'
    )
    for index in range(1, self._navigations + 1):
      action_runner.Wait(self._delay_seconds + rng.uniform(0, _JITTER_SECONDS))
      action_runner.ExecuteJavaScript('navigateToNextPage()')
      # This ignores errors caused by the navigation.
      action_runner.WaitForJavaScriptCondition(
        'window.__results !== undefined && '
        'window.__results.index === {{ index }} && '
        'window.__results.fcp !== undefined && '
        'window.__results.viewTransition !== undefined',
        index=index,
        timeout=30,
      )
      results = action_runner.EvaluateJavaScript('window.__results')
      self.fcp_samples.append(results['fcp'])
      if results['viewTransition'] != self.view_transition:
        self.view_transition_mismatches += 1


class ViewTransitionNavigationStorySet(story.StorySet):
  """Navigations with (vt_*) and without (no_vt_*) a view transition.

  The difference between the two is the cost of the view transition. Each
  story is tagged with "all", "vt" or "no_vt", and "fast" or "slow", e.g. for
  pinpoint try jobs, which need a story or story tags.
  """

  def __init__(self, smoke_test_mode=False):
    super().__init__()
    navigations = _SMOKE_TEST_NAVIGATIONS if smoke_test_mode else _NAVIGATIONS
    # The delay after the FCP of a page before navigating to the next page.
    delays = (
      ('fast', 0.5),
      ('slow', 1.5),
    )
    for speed, delay_seconds in delays:
      for view_transition in (True, False):
        kind = 'vt' if view_transition else 'no_vt'
        self.AddStory(
          _NavigationPage(
            self,
            name='%s_%s' % (kind, speed),
            tags=['all', kind, speed],
            view_transition=view_transition,
            delay_seconds=delay_seconds,
            navigations=navigations,
          )
        )
