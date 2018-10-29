# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from tracing.metrics import metric_runner
from tracing.value import histogram as histogram_module

def AddTBMv2RenderingMetrics(trace_value, results, import_experimental_metrics):
  mre_result = metric_runner.RunMetric(
      trace_value.filename, metrics=['renderingMetric'],
      extra_import_options={'trackDetailedModelStats': True},
      report_progress=False, canonical_url=results.telemetry_info.trace_url)

  for f in mre_result.failures:
    results.Fail(f.stack)

  histograms = []
  for histogram in mre_result.pairs.get('histograms', []):
    if (import_experimental_metrics or
        histogram.get('name', '').find('_tbmv2') < 0):
      histograms.append(histogram)
  results.ImportHistogramDicts(histograms, import_immediately=False)

def ExtractStat(results):
  stat = {}
  for histogram_dict in results.AsHistogramDicts():
    # It would be nicer if instead of converting results._histograms to dicts
    # and then parsing them back in the following line, results had a getter
    # returning results._histograms. But, since this is a temporary code that
    # will be deleted after transitioning Smoothness to TBMv2, we don't change
    # page_test_results.py for a temporary usecase.
    if 'name' in histogram_dict:
      histogram = histogram_module.Histogram.FromDict(histogram_dict)
      if histogram.running is None:
        continue
      if histogram.name in stat:
        stat[histogram.name] = stat[histogram.name].Merge(histogram.running)
      else:
        stat[histogram.name] = histogram.running
  return stat
