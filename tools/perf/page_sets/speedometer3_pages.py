# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Speedometer 3 Web Interaction Benchmark Pages
"""
import re

from page_sets import press_story

_SPEEDOMETER_SUITES = (
    'TodoMVC-JavaScript-ES5',
    'TodoMVC-JavaScript-ES6-Webpack',
    'TodoMVC-WebComponents',
    'TodoMVC-React',
    'TodoMVC-React-Complex-DOM',
    'TodoMVC-React-Redux',
    'TodoMVC-Backbone',
    'TodoMVC-Angular',
    'TodoMVC-Vue',
    'TodoMVC-jQuery',
    'TodoMVC-Preact',
    'TodoMVC-Svelte',
    'TodoMVC-Lit',
    'NewsSite-Next',
    'NewsSite-Nuxt',
    'Editor-CodeMirror',
    'Editor-TipTap',
    'Charts-observable-plot',
    'Charts-chartjs',
    'React-Stockcharts-SVG',
    'Perf-Dashboard',
)


class _Speedometer3Story(press_story.PressStory):
  URL = 'file://index.html'

  def __init__(self,
               page_set,
               should_filter_suites,
               filtered_suite_names=None,
               iterations=None):
    super(_Speedometer3Story, self).__init__(page_set)
    self._should_filter_suites = should_filter_suites
    self._filtered_suite_names = filtered_suite_names
    self._iterations = iterations

  @staticmethod
  def GetSuites(suite_regex):
    if not suite_regex:
      return []
    exp = re.compile(suite_regex)
    return [name for name in _SPEEDOMETER_SUITES if exp.search(name)]

  def RunNavigateSteps(self, action_runner):
    url = self.file_path_url_with_scheme if self.is_file else self.url
    if not self._iterations:
      self._iterations = 10
      # A single iteration on android takes ~75 seconds, the benchmark times out
      # when running for 10 iterations.
      if action_runner.tab.browser.platform.GetOSName() == 'android':
        self._iterations = 3
    url = "%s?iterationCount=%s" % (url, self._iterations)
    action_runner.Navigate(
        url, script_to_evaluate_on_commit=self.script_to_evaluate_on_commit)

  def ExecuteTest(self, action_runner):
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()

    if self._should_filter_suites:
      action_runner.ExecuteJavaScript(
          """
        Suites.forEach(function(suite) {
          suite.disabled = {{ filtered_suites }}.indexOf(suite.name) == -1;
        });
      """,
          filtered_suites=self._filtered_suite_names)

    action_runner.ExecuteJavaScript("""
        // Store all the results in the benchmarkClient
        window.testDone = false;
        window.metrics = Object.create(null);
        const client = window.benchmarkClient;
        const clientCopy = {
          didFinishLastIteration: client.didFinishLastIteration,
        };
        client.didFinishLastIteration = function(metrics) {
            clientCopy.didFinishLastIteration.call(this, metrics);
            window.metrics = metrics
            window.testDone = true;
        };
        """)
    action_runner.ExecuteJavaScript("""
        if (window.startTest) {
          window.startTest();
        } else {
          // Interactive Runner fallback / old 3.0 fallback.
          let startButton = document.getElementById("runSuites") ||
              document.querySelector("start-tests-button") ||
              document.querySelector(".buttons button");
          startButton.click();
        }
        """)
    action_runner.WaitForJavaScriptCondition('testDone', timeout=600)

  def ParseTestResults(self, action_runner):
    # Extract the timings for each suite
    metrics = action_runner.EvaluateJavaScript(
        "(function() { return window.metrics })()")
    assert metrics, "Expected metrics dict but got: %s" % metrics
    UNIT_LOOKUP = {
        "ms": "ms_smallerIsBetter",
        "score": "unitless_biggerIsBetter",
    }
    for name, metric in metrics.items():
      self.AddMeasurement(name, UNIT_LOOKUP[metric["unit"]], metric["values"])


class Speedometer30Story(_Speedometer3Story):
  NAME = 'Speedometer30'


class Speedometer3Story(Speedometer30Story):
  NAME = 'Speedometer3'
