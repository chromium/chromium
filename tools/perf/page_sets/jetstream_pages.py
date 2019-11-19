# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json

from page_sets import press_story
from telemetry import story
from telemetry.util import statistics


class JetstreamStory(press_story.PressStory):
  URL = 'http://browserbench.org/JetStream/'
  NAME = 'JetStream'

  def __init__(self, ps):
    super(JetstreamStory, self).__init__(ps)
    self.script_to_evaluate_on_commit = """
        var __results = [];
        var __real_log = window.console.log;
        window.console.log = function() {
          __results.push(Array.prototype.join.call(arguments, ' '));
          __real_log.apply(this, arguments);
        }
        """

  def ExecuteTest(self, action_runner):
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()
    action_runner.EvaluateJavaScript('JetStream.start()')

  def ParseTestResults(self, action_runner):
    result = action_runner.WaitForJavaScriptCondition("""
        (function() {
          for (var i = 0; i < __results.length; i++) {
            if (!__results[i].indexOf('Raw results: ')) return __results[i];
          }
          return null;
        })();
        """, timeout=60*20)
    result = json.loads(result.partition(': ')[2])

    all_score_lists = []
    for k, v in result.iteritems():
      self.AddMeasurement(k.replace('.', '_'), 'score', v['result'])
      # Collect all test scores to compute geometric mean.
      for i, score in enumerate(v['result']):
        if len(all_score_lists) <= i:
          all_score_lists.append([])
        all_score_lists[i].append(score)
    all_scores = []
    for score_list in all_score_lists:
      all_scores.append(statistics.GeometricMean(score_list))
    self.AddMeasurement('Score', 'score', all_scores)


class JetstreamStorySet(story.StorySet):
  def __init__(self):
    super(JetstreamStorySet, self).__init__(
        archive_data_file='data/jetstream.json',
        cloud_storage_bucket=story.INTERNAL_BUCKET)

    self.AddStory(JetstreamStory(self))
