# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import story

from page_sets import press_story


class Speedometer1Story(press_story.PressStory):
  URL = 'http://browserbench.org/Speedometer/'

  enabled_suites = [
      'VanillaJS-TodoMVC',
      'EmberJS-TodoMVC',
      'BackboneJS-TodoMVC',
      'jQuery-TodoMVC',
      'AngularJS-TodoMVC',
      'React-TodoMVC',
      'FlightJS-TodoMVC',
  ]

  def ExecuteTest(self, action_runner):
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()
    iterationCount = 10
    # A single iteration on android takes ~75 seconds, the benchmark times out
    # when running for 10 iterations.
    if action_runner.tab.browser.platform.GetOSName() == 'android':
      iterationCount = 3

    action_runner.ExecuteJavaScript("""
        // Store all the results in the benchmarkClient
        benchmarkClient._measuredValues = []
        benchmarkClient.didRunSuites = function(measuredValues) {
          benchmarkClient._measuredValues.push(measuredValues);
          benchmarkClient._timeValues.push(measuredValues.total);
        };
        benchmarkClient.iterationCount = {{ count }};
        startTest();
        """,
                                    count=iterationCount)
    action_runner.WaitForJavaScriptCondition(
        'benchmarkClient._finishedTestCount == benchmarkClient.testsCount',
        timeout=600)

  def ParseTestResults(self, action_runner):
    self.AddJavaScriptMeasurement('Total', 'ms', 'benchmarkClient._timeValues')
    self.AddJavaScriptMeasurement(
        'RunsPerMinute', 'score',
        '[parseFloat(document.getElementById("result-number").innerText)];')

    # Extract the timings for each suite
    for suite_name in self.enabled_suites:
      self.AddJavaScriptMeasurement(suite_name,
                                    'ms',
                                    """
          var suite_times = [];
          for(var i = 0; i < benchmarkClient.iterationCount; i++) {
            suite_times.push(
                benchmarkClient._measuredValues[i].tests[{{ key }}].total);
          };
          suite_times;
          """,
                                    key=suite_name)


class Speedometer1StorySet(story.StorySet):
  def __init__(self):
    super(Speedometer1StorySet,
          self).__init__(archive_data_file='data/speedometer.json',
                         cloud_storage_bucket=story.PUBLIC_BUCKET)

    self.AddStory(Speedometer1Story(self))
