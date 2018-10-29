# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Apple's JetStream benchmark.

JetStream combines a variety of JavaScript benchmarks, covering a variety of
advanced workloads and programming techniques, and reports a single score that
balances them using geometric mean.

Each benchmark measures a distinct workload, and no single optimization
technique is sufficient to speed up all benchmarks. Latency tests measure that
a web application can start up quickly, ramp up to peak performance quickly,
and run smoothly without interruptions. Throughput tests measure the sustained
peak performance of a web application, ignoring ramp-up time and spikes in
smoothness. Some benchmarks demonstrate trade-offs, and aggressive or
specialized optimization for one benchmark might make another benchmark slower.
"""

import json
import os

from core import perf_benchmark

from telemetry import benchmark
from telemetry import page as page_module
from telemetry.page import legacy_page_test
from telemetry import story
from telemetry.util import statistics
from telemetry.value import list_of_scalar_values


class _JetstreamMeasurement(legacy_page_test.LegacyPageTest):

  def __init__(self):
    super(_JetstreamMeasurement, self).__init__()

  def WillNavigateToPage(self, page, tab):
    page.script_to_evaluate_on_commit = """
        var __results = [];
        var __real_log = window.console.log;
        window.console.log = function() {
          __results.push(Array.prototype.join.call(arguments, ' '));
          __real_log.apply(this, arguments);
        }
        """

  def ValidateAndMeasurePage(self, page, tab, results):
    del page  # unused
    tab.WaitForDocumentReadyStateToBeComplete()
    tab.EvaluateJavaScript('JetStream.start()')
    result = tab.WaitForJavaScriptCondition("""
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
      results.AddValue(list_of_scalar_values.ListOfScalarValues(
          results.current_page, k.replace('.', '_'), 'score', v['result'],
          important=False))
      # Collect all test scores to compute geometric mean.
      for i, score in enumerate(v['result']):
        if len(all_score_lists) <= i:
          all_score_lists.append([])
        all_score_lists[i].append(score)
    all_scores = []
    for score_list in all_score_lists:
      all_scores.append(statistics.GeometricMean(score_list))
    results.AddSummaryValue(list_of_scalar_values.ListOfScalarValues(
        None, 'Score', 'score', all_scores))

@benchmark.Info(emails=['hablich@chromium.org'],
                component='Blink>JavaScript')
class Jetstream(perf_benchmark.PerfBenchmark):
  test = _JetstreamMeasurement

  @classmethod
  def Name(cls):
    return 'jetstream'

  def CreateStorySet(self, options):
    ps = story.StorySet(
        archive_data_file='../page_sets/data/jetstream.json',
        base_dir=os.path.dirname(os.path.abspath(__file__)),
        cloud_storage_bucket=story.INTERNAL_BUCKET)
    ps.AddStory(page_module.Page(
        'http://browserbench.org/JetStream/', ps, ps.base_dir,
        make_javascript_deterministic=False,
        name='http://browserbench.org/JetStream/'))
    return ps
