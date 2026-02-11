# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import statistics
from urllib.parse import urlencode
from page_sets import press_story
from telemetry import story


class _JetStream3Story(press_story.PressStory):
  # TODO: update to new URL once release.
  URL = 'https://browserben.ch/jetstream/main/'

  def __init__(self, page_set, test_list):
    url = self.URL
    if test_list:
      query = urlencode({'test': test_list})
      url = f"{url}?{query}"

    super(_JetStream3Story, self).__init__(page_set, url=url)

  def ExecuteTest(self, action_runner):
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()
    # Wait till the elements with selector "#results>.benchmark" are available
    # as they are required for running "JetStream.start()"
    action_runner.WaitForJavaScriptCondition("JetStream.isReady",
                                             timeout=60 * 2)
    action_runner.EvaluateJavaScript('JetStream.start()')

  def ParseTestResults(self, action_runner):
    # JetStream3 is using document object to set done of benchmark runs.
    action_runner.WaitForJavaScriptCondition("JetStream.isDone",
                                             timeout=60 * 30)
    result = action_runner.EvaluateJavaScript(
        "JetStream.simpleResultsObject();")
    sub_results_dict = collections.defaultdict(list)
    for k, v in result.items():
      # Replace '.' in the benchmark name, because '.' is interpreted
      # as a sub-category of the metric
      benchmark = str(k).replace('.', '_')
      for sub_k, sub_v in v.items():
        self.AddMeasurement('%s.%s' % (benchmark, sub_k), 'score', sub_v)
        sub_results_dict[sub_k].append(sub_v)

    for sub_k, values in sub_results_dict.items():
      self.AddMeasurement('Total.%s' % sub_k, 'score',
                          statistics.geometric_mean(values))


class _JetStream3StorySet(story.StorySet):

  def __init__(self, test_list=None):
    super(_JetStream3StorySet,
          self).__init__(archive_data_file='data/jetstream3.json',
                         cloud_storage_bucket=story.INTERNAL_BUCKET)
    self.AddStory(self._STORY_CLS(self, test_list))


class JetStream30Story(_JetStream3Story):
  NAME = 'JetStream30'


class JetStream30StorySet(_JetStream3StorySet):
  _STORY_CLS = JetStream30Story


class JetStream3Story(_JetStream3Story):
  NAME = 'JetStream3'


class JetStream3StorySet(_JetStream3StorySet):
  _STORY_CLS = JetStream3Story
