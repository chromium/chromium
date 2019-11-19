# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from cli_tools.pinpoint_cli import histograms_df
from core.external_modules import pandas

from tracing.value import histogram
from tracing.value import histogram_set
from tracing.value.diagnostics import date_range
from tracing.value.diagnostics import generic_set


def TestHistogram(name, units, values, **kwargs):
  def DiagnosticValue(value):
    if isinstance(value, (int, long)):
      return date_range.DateRange(value)
    elif isinstance(value, list):
      return generic_set.GenericSet(value)
    else:
      raise NotImplementedError(type(value))

  hist = histogram.Histogram(name, units)
  hist.diagnostics.update(
      (key, DiagnosticValue(value)) for key, value in kwargs.iteritems())
  for value in values:
    hist.AddSample(value)
  return hist


@unittest.skipIf(pandas is None, 'pandas not available')
class TestHistogramsDf(unittest.TestCase):
  def testIterRows(self):
    run1 = {'benchmarkStart': 1234567890000, 'labels': ['run1'],
            'benchmarks': ['system_health'], 'deviceIds': ['device1']}
    # Second run on same device ten minutes later.
    run2 = {'benchmarkStart': 1234567890000 + 600000, 'labels': ['run2'],
            'benchmarks': ['system_health'], 'deviceIds': ['device1']}
    hists = histogram_set.HistogramSet([
        TestHistogram('startup', 'ms', [8, 10, 12], stories=['story1'],
                      traceUrls=['http://url/to/trace1'], **run1),
        TestHistogram('memory', 'sizeInBytes', [256], stories=['story2'],
                      traceUrls=['http://url/to/trace2'], **run1),
        TestHistogram('memory', 'sizeInBytes', [512], stories=['story2'],
                      traceUrls=['http://url/to/trace3'], **run2),
    ])

    expected = [
        ('startup', 'ms', 10.0, 2.0, 3, 'run1', 'system_health',
         'story1', '2009-02-13 23:31:30', 'device1', 'http://url/to/trace1'),
        ('memory', 'sizeInBytes', 256.0, 0.0, 1, 'run1', 'system_health',
         'story2', '2009-02-13 23:31:30', 'device1', 'http://url/to/trace2'),
        ('memory', 'sizeInBytes', 512.0, 0.0, 1, 'run2', 'system_health',
         'story2', '2009-02-13 23:41:30', 'device1', 'http://url/to/trace3'),
    ]
    self.assertItemsEqual(histograms_df.IterRows(hists.AsDicts()), expected)

  def testDataFrame(self):
    run1 = {'benchmarkStart': 1234567890000, 'labels': ['run1'],
            'benchmarks': ['system_health'], 'deviceIds': ['device1']}
    # Second run on same device ten minutes later.
    run2 = {'benchmarkStart': 1234567890000 + 600000, 'labels': ['run2'],
            'benchmarks': ['system_health'], 'deviceIds': ['device1']}
    hists = histogram_set.HistogramSet([
        TestHistogram('startup', 'ms', [8, 10, 12], stories=['story1'],
                      traceUrls=['http://url/to/trace1'], **run1),
        TestHistogram('memory', 'sizeInBytes', [256], stories=['story2'],
                      traceUrls=['http://url/to/trace2'], **run1),
        TestHistogram('memory', 'sizeInBytes', [384], stories=['story2'],
                      traceUrls=['http://url/to/trace3'], **run2),
    ])
    df = histograms_df.DataFrame(hists.AsDicts())

    # Poke at the data frame and check a few known facts about our fake data:
    # It has 3 histograms.
    self.assertEqual(len(df), 3)
    # The benchmark has two stories.
    self.assertItemsEqual(df['story'].unique(), ['story1', 'story2'])
    # We recorded three traces.
    self.assertEqual(len(df['trace_url'].unique()), 3)
    # All benchmarks ran on the same device.
    self.assertEqual(len(df['device_id'].unique()), 1)
    # There is a memory regression between runs 1 and 2.
    memory = df.set_index(['name', 'run_label']).loc['memory']['mean']
    self.assertEqual(memory['run2'] - memory['run1'], 128.0)
    # Ten minutes passed between the two benchmark runs.
    self.assertEqual(df['benchmark_start'].max() - df['benchmark_start'].min(),
                     pandas.Timedelta('10 minutes'))
