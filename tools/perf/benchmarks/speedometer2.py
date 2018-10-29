# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Apple's Speedometer 2 performance benchmark.
"""

import os
import re

from core import path_util
from core import perf_benchmark

from telemetry import benchmark
from telemetry import page as page_module
from telemetry.page import legacy_page_test
from telemetry import story
from telemetry.value import list_of_scalar_values


_SPEEDOMETER_DIR = os.path.join(path_util.GetChromiumSrcDir(),
    'third_party', 'blink', 'perf_tests', 'speedometer')
_SPEEDOMETER_SUITE_NAME_BASE = '{0}-TodoMVC'
_SPEEDOMETER_SUITES = [
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
  'Flight'
]


class Speedometer2Measurement(legacy_page_test.LegacyPageTest):
  def __init__(self, should_filter_suites, filtered_suite_names=None,
               enable_smoke_test_mode=False):
    super(Speedometer2Measurement, self).__init__()
    self.should_filter_suites_ = should_filter_suites
    self.filtered_suites_ = filtered_suite_names
    self.enable_smoke_test_mode = enable_smoke_test_mode

  def ValidateAndMeasurePage(self, page, tab, results):
    tab.WaitForDocumentReadyStateToBeComplete()
    iterationCount = 10
    # A single iteration on android takes ~75 seconds, the benchmark times out
    # when running for 10 iterations.
    if tab.browser.platform.GetOSName() == 'android':
      iterationCount = 3
    # For a smoke test one iteration is sufficient
    if self.enable_smoke_test_mode:
      iterationCount = 1

    if self.should_filter_suites_:
      tab.ExecuteJavaScript("""
        Suites.forEach(function(suite) {
          suite.disabled = {{ filtered_suites }}.indexOf(suite.name) < 0;
        });
      """, filtered_suites=self.filtered_suites_)

    enabled_suites = tab.EvaluateJavaScript("""
      (function() {
        var suitesNames = [];
        Suites.forEach(function(s) {
          if (!s.disabled)
            suitesNames.push(s.name);
        });
        return suitesNames;
       })();""")

    tab.ExecuteJavaScript("""
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
    tab.WaitForJavaScriptCondition('testDone', timeout=600)

    if not self.should_filter_suites_:
      results.AddValue(list_of_scalar_values.ListOfScalarValues(
          page, 'Total', 'ms',
          tab.EvaluateJavaScript('suiteValues.map(each => each.total)'),
          important=True))
      results.AddValue(list_of_scalar_values.ListOfScalarValues(
          page, 'RunsPerMinute', 'score',
          tab.EvaluateJavaScript('suiteValues.map(each => each.score)'),
          important=True))

    # Extract the timings for each suite
    for suite_name in enabled_suites:
      results.AddValue(list_of_scalar_values.ListOfScalarValues(
          page, suite_name, 'ms',
          tab.EvaluateJavaScript("""
              var suite_times = [];
              for(var i = 0; i < iterationCount; i++) {
                suite_times.push(
                    suiteValues[i].tests[{{ key }}].total);
              };
              suite_times;
              """,
              key=suite_name), important=False))


@benchmark.Info(emails=['hablich@chromium.org'],
                component='Blink')
class Speedometer2(perf_benchmark.PerfBenchmark):
  """Speedometer2 Benchmark.

  Runs all the speedometer 2 suites by default. Add --suite=<regex> to filter
  out suites, and only run suites whose names are matched by the regular
  expression provided.
  """

  enable_smoke_test_mode = False

  @classmethod
  def Name(cls):
    return 'speedometer2'

  @staticmethod
  def GetFullSuiteName(name):
    return _SPEEDOMETER_SUITE_NAME_BASE.format(name)

  @staticmethod
  def GetSuites(suite_regex):
    if not suite_regex:
      return []
    exp = re.compile(suite_regex)
    return [name for name in _SPEEDOMETER_SUITES
            if exp.search(Speedometer2.GetFullSuiteName(name))]

  def CreatePageTest(self, options):
      should_filter_suites = bool(options.suite)
      filtered_suite_names = map(Speedometer2.GetFullSuiteName,
          Speedometer2.GetSuites(options.suite))
      return Speedometer2Measurement(should_filter_suites, filtered_suite_names,
                                     self.enable_smoke_test_mode)

  def CreateStorySet(self, options):
    ps = story.StorySet(base_dir=_SPEEDOMETER_DIR)
    ps.AddStory(page_module.Page(
       'file://InteractiveRunner.html', ps, ps.base_dir, name='Speedometer2'))
    return ps

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    parser.add_option('--suite', type="string",
                      help="Only runs suites that match regex provided")

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    if args.suite:
      try:
        if not Speedometer2.GetSuites(args.suite):
          raise parser.error('--suite: No matches.')
      except re.error:
        raise parser.error('--suite: Invalid regex.')


@benchmark.Info(emails=['hablich@chromium.org'],
                component='Blink')
class V8Speedometer2Future(Speedometer2):
  """Speedometer2 benchmark with the V8 flag --future.

  Shows the performance of upcoming V8 VM features.
  """

  @classmethod
  def Name(cls):
    return 'speedometer2-future'

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-features=V8VmFuture')
