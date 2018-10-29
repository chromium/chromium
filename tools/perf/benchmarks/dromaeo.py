# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import math
import os

from core import perf_benchmark

from telemetry import benchmark
from telemetry import page as page_module
from telemetry.page import legacy_page_test
from telemetry import story
from telemetry.value import scalar


class _DromaeoMeasurement(legacy_page_test.LegacyPageTest):

  def __init__(self):
    super(_DromaeoMeasurement, self).__init__()

  def ValidateAndMeasurePage(self, page, tab, results):
    tab.WaitForJavaScriptCondition(
        'window.document.getElementById("pause") &&' +
        'window.document.getElementById("pause").value == "Run"',
        timeout=120)

    # Start spying on POST request that will report benchmark results, and
    # intercept result data.
    tab.ExecuteJavaScript("""
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
    tab.ExecuteJavaScript('window.document.getElementById("pause").click();')

    tab.WaitForJavaScriptCondition('!!window.results_', timeout=600)

    score = json.loads(tab.EvaluateJavaScript('window.results_ || "[]"'))

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

    suffix = page.url[page.url.index('?') + 1:]

    def AddResult(name, value):
      important = False
      if name == suffix:
        important = True
      results.AddValue(scalar.ScalarValue(
          results.current_page, Escape(name), 'runs/s', value, important))

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


@benchmark.Info(component='Blink>Bindings',
                emails=['jbroman@chromium.org',
                         'yukishiino@chromium.org',
                         'haraken@chromium.org'])
class DromaeoBenchmark(perf_benchmark.PerfBenchmark):

  test = _DromaeoMeasurement

  @classmethod
  def Name(cls):
    return 'dromaeo'

  def CreateStorySet(self, options):
    archive_data_file = '../page_sets/data/dromaeo.json'
    ps = story.StorySet(
        archive_data_file=archive_data_file,
        base_dir=os.path.dirname(os.path.abspath(__file__)),
        cloud_storage_bucket=story.PUBLIC_BUCKET)
    for query_param in ['dom-attr', 'dom-modify', 'dom-query', 'dom-traverse']:
      url = 'http://dromaeo.com?%s' % query_param
      ps.AddStory(page_module.Page(
          url, ps, ps.base_dir, make_javascript_deterministic=False, name=url))
    return ps
