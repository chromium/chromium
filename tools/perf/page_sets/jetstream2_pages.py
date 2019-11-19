# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets import press_story
from telemetry import story


class Jetstream2Story(press_story.PressStory):
  URL = 'http://browserbench.org/JetStream/'
  NAME = 'JetStream2'

  def __init__(self, ps):
    super(Jetstream2Story, self).__init__(ps)

  def ExecuteTest(self, action_runner):
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()
    # Wait till the elements with selector "#results>.benchmark" are available
    # as they are required for running "JetStream.start()"
    action_runner.WaitForJavaScriptCondition("""
        document.querySelectorAll("#results>.benchmark").length > 0
        """, timeout=5)
    action_runner.EvaluateJavaScript('JetStream.start()')

  def ParseTestResults(self, action_runner):
    # JetStream2 is using document object to set done of benchmark runs.
    action_runner.WaitForJavaScriptCondition("""
        (function() {
          let summaryElement = document.getElementById("result-summary");
          return (summaryElement.classList.contains('done'));
        })();
        """, timeout=60*20)
    # JetStream2 calculates scores for each benchmark across iterations
    # so for each benchmark, return its calculated score and sub-results(
    # For JavaScript benchmarks, it's scores of "First", "Worst", "Average"
    # For WSL benchmarks, it's scores of "Stdlib", "MainRun"
    # For Wasm benchmarks, it's scores of "Startup", "Runtime"
    # Also use Javascript to calculate the geomean of all scores
    result, score = action_runner.EvaluateJavaScript("""
        (function() {
          let result = {};
          let allScores = [];
          for (let benchmark of JetStream.benchmarks) {
            const subResults = {};
            const subTimes = benchmark.subTimes();
            for (const name in subTimes) {
                subResults[name] = subTimes[name];
            };
            result[benchmark.name] = {
                "Score" : benchmark.score,
                "Iterations" : benchmark.iterations,
                "SubResults": subResults,
            };
            allScores.push(benchmark.score);
          };
          return [result, geomean(allScores)];
        })();"""
    )

    self.AddMeasurement('Score', 'score', score)
    for k, v in result.iteritems():
      # Replace '.' in the benchmark name, because '.' is interpreted
      # as a sub-category of the metric
      benchmark = str(k).replace('.', '_')
      self.AddMeasurement(
          benchmark, 'score', v['Score'],
          description='Geometric mean of the iterations')
      self.AddMeasurement(
          '%s.Iterations' % benchmark, 'count', v['Iterations'],
          description='Total number of iterations')
      for sub_k, sub_v in v['SubResults'].iteritems():
        self.AddMeasurement('%s.%s' % (benchmark, sub_k), 'score', sub_v)


class Jetstream2StorySet(story.StorySet):
  def __init__(self):
    super(Jetstream2StorySet, self).__init__(
        archive_data_file='data/jetstream2.json',
        cloud_storage_bucket=story.INTERNAL_BUCKET)

    self.AddStory(Jetstream2Story(self))
