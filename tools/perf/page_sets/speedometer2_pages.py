# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Apple's Speedometer 2 performance benchmark pages
"""
import re

from page_sets import press_story

_SPEEDOMETER_SUITE_NAME_BASE = '{0}-TodoMVC'
_SPEEDOMETER_SUITES = (
    'VanillaJS',
    'Vanilla-ES2015',
    'Vanilla-ES2015-Babel-Webpack',
    'React',
    'React-Redux',
    'EmberJS',
    'EmberJS-Debug',
    'BackboneJS',
    'AngularJS',
    'Angular2-TypeScript',
    'VueJS',
    'jQuery',
    'Preact',
    'Inferno',
    'Elm',
    'Flight',
)


class _Speedometer2Story(press_story.PressStory):
  URL = 'file://InteractiveRunner.html'

  def __init__(self,
               page_set,
               should_filter_suites,
               filtered_suite_names=None,
               iterations=None):
    super(_Speedometer2Story, self).__init__(page_set)
    self._should_filter_suites = should_filter_suites
    self._filtered_suite_names = filtered_suite_names
    self._iterations = iterations
    self._enabled_suites = []

  @staticmethod
  def GetFullSuiteName(name):
    return _SPEEDOMETER_SUITE_NAME_BASE.format(name)

  @staticmethod
  def GetSuites(suite_regex):
    if not suite_regex:
      return []
    exp = re.compile(suite_regex)
    return [
        name for name in _SPEEDOMETER_SUITES
        if exp.search(_Speedometer2Story.GetFullSuiteName(name))
    ]

  def ExecuteTest(self, action_runner):
    DEFAULT_ITERATIONS = 10

    action_runner.tab.WaitForDocumentReadyStateToBeComplete()
    iterationCount = (self._iterations
                      if self._iterations is not None else DEFAULT_ITERATIONS)

    if self._should_filter_suites:
      action_runner.ExecuteJavaScript(
          """
        Suites.forEach(function(suite) {
          suite.disabled = {{ filtered_suites }}.indexOf(suite.name) == -1;
        });
      """,
          filtered_suites=self._filtered_suite_names)

    self._enabled_suites = action_runner.EvaluateJavaScript("""
      (function() {
        var suitesNames = [];
        Suites.forEach(function(s) {
          if (!s.disabled)
            suitesNames.push(s.name);
        });
        return suitesNames;
       })();""")

    action_runner.ExecuteJavaScript("""
        // Store all the results in the benchmarkClient
        var testDone = false;
        var iterationCount = {{ count }};
        var benchmarkClient = {};
        var suiteValues = [];
        benchmarkClient.didRunSuites = function(measuredValues) {
          suiteValues.push(measuredValues);
        };
        benchmarkClient.didFinishLastIteration = function () {
          testDone = true;
        };
        var runner = new BenchmarkRunner(Suites, benchmarkClient);
        runner.runMultipleIterations(iterationCount);
        """,
        count=iterationCount)
    action_runner.WaitForJavaScriptCondition('testDone', timeout=600)

  def ParseTestResults(self, action_runner):
    if not self._should_filter_suites:
      self.AddJavaScriptMeasurement(
          'Total', 'ms_smallerIsBetter', 'suiteValues.map(each => each.total)')
      self.AddJavaScriptMeasurement(
          'RunsPerMinute', 'unitless_biggerIsBetter',
          'suiteValues.map(each => each.score)')

    # Extract the timings for each suite
    for suite_name in self._enabled_suites:
      self.AddJavaScriptMeasurement(
          suite_name, 'ms_smallerIsBetter',
          """
          var suite_times = [];
          for(var i = 0; i < iterationCount; i++) {
            suite_times.push(
                suiteValues[i].tests[{{ key }}].total);
          };
          suite_times;
          """,
          key=suite_name)


class Speedometer20Story(_Speedometer2Story):
  NAME = 'Speedometer20'


class Speedometer21Story(_Speedometer2Story):
  NAME = 'Speedometer21'


class Speedometer2Story(Speedometer20Story):
  NAME = 'Speedometer2'
