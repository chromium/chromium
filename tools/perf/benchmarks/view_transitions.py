# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Benchmarks for cross-document view transitions."""

from core import perf_benchmark
from telemetry import benchmark
from telemetry.page import legacy_page_test

from page_sets import view_transitions_story_set

_EMAILS = [
  'vmpstr@chromium.org',
  'blink-interactions-team@google.com',
]
_COMPONENT = 'Blink>ViewTransitions'
_DOCUMENTATION_URL = (
  'https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/'
  'renderer/core/view_transition/README.md'
)


class _NavigationMeasurement(legacy_page_test.LegacyPageTest):
  """Reports the FCP of the pages that were navigated to by a story."""

  def ValidateAndMeasurePage(self, page, tab, results):
    del tab  # unused
    if not page.fcp_samples:
      raise legacy_page_test.MeasurementFailure('No FCP samples were recorded')
    if page.view_transition_mismatches:
      raise legacy_page_test.MeasurementFailure(
        '%d of %d navigations %s a view transition'
        % (
          page.view_transition_mismatches,
          len(page.fcp_samples),
          'did not have' if page.view_transition else 'had',
        )
      )
    results.AddMeasurement(
      'navigation_to_first_contentful_paint',
      'ms_smallerIsBetter',
      page.fcp_samples,
      description=(
        'Time from the start of a navigation to the first '
        'contentful paint of the new page.'
      ),
    )


@benchmark.Info(
  emails=_EMAILS, component=_COMPONENT, documentation_url=_DOCUMENTATION_URL
)
class ViewTransitionsNavigation(perf_benchmark.PerfBenchmark):
  """Measures same-origin navigations with and without a view transition.

  Compare vt_* and no_vt_* stories for the cost of a cross-document view
  transition.
  """

  # benchmark_smoke_unittest.py sets this to make the stories navigate fewer
  # times. It only sets it if the benchmark class already has it.
  enable_smoke_test_mode = False

  def CreateStorySet(self, options):
    return view_transitions_story_set.ViewTransitionNavigationStorySet(
      smoke_test_mode=self.enable_smoke_test_mode
    )

  def CreatePageTest(self, options):
    return _NavigationMeasurement()

  @classmethod
  def Name(cls):
    return 'UNSCHEDULED_view_transitions.navigation'
