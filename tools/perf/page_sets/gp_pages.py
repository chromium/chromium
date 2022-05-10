# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""GrandPrix performance benchmark pages
"""
from telemetry import story
from page_sets import press_story
from page_sets.system_health import platforms

_GP_PAGE_ALL = 'All'
_GP_SCORE_METRIC = 'Score'
_GP_TOPLEVEL_METRICS = ('Total', _GP_SCORE_METRIC)

_GP_PAGES = [
    'Vanilla-HTML-TodoMVC', 'Vanilla-TodoMVC', 'Preact-TodoMVC',
    'Preact-TodoMVC-Modern', 'React-TodoMVC', 'Elm-TodoMVC', 'Rust-Yew-TodoMVC',
    'Hydration-Preact', 'Scroll-Windowing-React', 'Monaco-Editor',
    'Monaco-Syntax-Highlight', 'React-Stockcharts', 'React-Stockcharts-SVG',
    'Leaflet-Fractal', 'SVG-UI', 'Proxx-Tables', 'Proxx-Tables-Lit',
    'Proxx-Tables-Canvas', _GP_PAGE_ALL
]


class GPStory(press_story.PressStory):
  BASE_URL = 'http://localhost:8000/?suites={suite}&sandbox=none&unit=ms'\
             '&viewport={viewport}&headless=1'
  NAME = 'GP'

  def __init__(self,
               page_set,
               name,
               iterations,
               viewport,
               skip_iteration_metrics=False):
    super(GPStory, self).__init__(page_set,
                                  url=self.BASE_URL.format(suite=name,
                                                           viewport=viewport),
                                  name=name)
    self._iterations = iterations or 10
    self._skip_iteration_metrics = skip_iteration_metrics

  def ExecuteTest(self, action_runner):
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()
    action_runner.Wait(1)
    action_runner.EvaluateJavaScript("""
        globalThis.runner.configure({
          iterations: {{ iterations }}
        });
        globalThis.runner.run(); // returns a Promise
      """,
                                     promise=True,
                                     timeout=30,
                                     iterations=self._iterations)

  def ParseTestResults(self, action_runner):
    metrics = action_runner.EvaluateJavaScript(
        "Object.keys(globalThis.results.metrics)")
    for metric_name in metrics:
      if self._skip_iteration_metrics and metric_name.startswith('Iteration-'):
        continue
      unit = 'ms_smallerIsBetter'
      if metric_name == _GP_SCORE_METRIC:
        unit = 'unitless_biggerIsBetter'
      reported_name = metric_name
      # if self.name == _GP_PAGE_ALL:
      #   reported_name = "All-%s" % metric_name
      # else:
      #   if metric_name in _GP_TOPLEVEL_METRICS:
      #     if metric_name == _GP_SCORE_METRIC:
      #       reported_name = "%s-%s" % (self.name, metric_name)
      #     else:
      #       # Only report top-level metrics
      #       continue
      self.AddJavaScriptMeasurement(
          reported_name, unit,
          'globalThis.results.metrics["%s"].values' % metric_name)


class _GPStorySet(story.StorySet):
  def __init__(self, iteration_count, viewport, show_iteration_metrics=False):
    super(_GPStorySet,
          self).__init__(archive_data_file='data/GP.json',
                         cloud_storage_bucket=story.INTERNAL_BUCKET)
    suites = _GP_PAGES
    for name in suites:
      self.AddStory(
          GPStory(self, name, iteration_count, viewport,
                  show_iteration_metrics))


class GPDesktopStorySet2022(_GPStorySet):
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY

  def __init__(self, iteration_count, show_iteration_metrics):
    super(GPDesktopStorySet2022,
          self).__init__(iteration_count,
                         viewport="2000x1000",
                         show_iteration_metrics=show_iteration_metrics)


class GPMobileStorySet2022(_GPStorySet):
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY

  def __init__(self, iteration_count, show_iteration_metrics):
    super(GPMobileStorySet2022,
          self).__init__(iteration_count,
                         viewport="1000x2000",
                         show_iteration_metrics=show_iteration_metrics)
