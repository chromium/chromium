# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import math

from telemetry import story

from page_sets import press_story

class DromaeoStory(press_story.PressStory):

  def __init__(self, url, ps):
    self.URL = url
    super(DromaeoStory, self).__init__(ps)


  def ExecuteTest(self, action_runner):
    action_runner.WaitForJavaScriptCondition(
        'window.document.getElementById("pause") &&' +
        'window.document.getElementById("pause").value == "Run"',
        timeout=120)

    # Start spying on POST request that will report benchmark results, and
    # intercept result data.
    action_runner.ExecuteJavaScript("""
        (function() {
          var real_jquery_ajax_ = window.jQuery;
          window.results_ = "";
          window.jQuery.ajax = function(request) {
            if (request.url == "store.php") {
              window.results_ = decodeURIComponent(request.data);
              window.results_ = window.results_.substring(
                window.results_.indexOf("=") + 1,
                window.results_.lastIndexOf("&"));
              real_jquery_ajax_(request);
            }
          };
        })();""")
    # Starts benchmark.
    action_runner.ExecuteJavaScript(
        'window.document.getElementById("pause").click();')

    action_runner.WaitForJavaScriptCondition('!!window.results_', timeout=600)


  def ParseTestResults(self, action_runner):
    score = json.loads(
        action_runner.EvaluateJavaScript('window.results_ || "[]"'))

    def Escape(k):
      chars = [' ', '.', '-', '/', '(', ')', '*']
      for c in chars:
        k = k.replace(c, '_')
      return k

    def AggregateData(container, key, value):
      if key not in container:
        container[key] = {'count': 0, 'sum': 0}
      container[key]['count'] += 1
      container[key]['sum'] += math.log(value)

    def AddResult(name, value):
      self.AddMeasurement(Escape(name), 'unitless_biggerIsBetter', [value])

    aggregated = {}
    for data in score:
      AddResult('%s/%s' % (data['collection'], data['name']),
                data['mean'])

      top_name = data['collection'].split('-', 1)[0]
      AggregateData(aggregated, top_name, data['mean'])

      collection_name = data['collection']
      AggregateData(aggregated, collection_name, data['mean'])

    for key, value in aggregated.iteritems():
      AddResult(key, math.exp(value['sum'] / value['count']))


class DromaeoStorySet(story.StorySet):
  def __init__(self):
    super(DromaeoStorySet, self).__init__(
        archive_data_file='../page_sets/data/dromaeo.json',
        cloud_storage_bucket=story.PUBLIC_BUCKET)

    for query_param in ['dom-attr', 'dom-modify', 'dom-query', 'dom-traverse']:
      url = 'http://dromaeo.com?%s' % query_param
      self.AddStory(DromaeoStory(url, self))
