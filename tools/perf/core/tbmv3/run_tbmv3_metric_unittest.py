# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Integration tests for run_tbmv3_metric.py.

These test check run_tbmv3_metric works end to end, using the real trace
processor shell.
"""

import json
import os
import shutil
import tempfile
import unittest

from core.tbmv3 import run_tbmv3_metric
from core.tbmv3 import trace_processor

from tracing.value import histogram_set

# For testing the TBMv3 workflow we use dummy_metric defined in
# tools/perf/core/tbmv3/metrics/dummy_metric_*.
# This metric ignores the trace data and outputs a histogram with
# the following name and unit:
DUMMY_HISTOGRAM_NAME = 'dummy::simple_field'
DUMMY_HISTOGRAM_UNIT = 'count_smallerIsBetter'


class RunTbmv3MetricIntegrationTests(unittest.TestCase):
  def setUp(self):
    self.output_dir = tempfile.mkdtemp()
    self.trace_path = self.CreateEmptyProtoTrace()
    self.outfile_path = os.path.join(self.output_dir, 'out.json')

  def tearDown(self):
    shutil.rmtree(self.output_dir)

  def CreateEmptyProtoTrace(self):
    """Create an empty file as a proto trace."""
    with tempfile.NamedTemporaryFile(
        dir=self.output_dir, delete=False) as trace_file:
      # Open temp file and close it so it's written to disk.
      pass
    return trace_file.name

  def testRunTbmv3MetricOnDummyMetric(self):
    run_tbmv3_metric.Main([
        '--trace', self.trace_path,
        '--metric', 'dummy_metric',
        '--outfile', self.outfile_path,
    ])

    with open(self.outfile_path) as f:
      results = json.load(f)

    out_histograms = histogram_set.HistogramSet()
    out_histograms.ImportDicts(results)

    hist = out_histograms.GetHistogramNamed(DUMMY_HISTOGRAM_NAME)
    self.assertEqual(hist.unit, DUMMY_HISTOGRAM_UNIT)
    self.assertEqual(hist.num_values, 1)
    self.assertEqual(hist.average, 42)

  def testRunAllTbmv3Metrics(self):
    """Run all existing TBMv3 metrics on an empty trace.

    This test checks for syntax errors in SQL and proto files.
    """
    for filename in os.listdir(trace_processor.METRICS_PATH):
      name, ext = os.path.splitext(filename)
      if ext == '.sql':
        run_tbmv3_metric.Main([
            '--trace', self.trace_path,
            '--metric', name,
            '--outfile', self.outfile_path,
        ])

  def testRunInternalTBMv3Metric(self):
    """Run metric that is compiled into Trace Processor."""
    # This won't produce any histograms because trace_metadata proto is not
    # annotated. Check only that it doesn't throw errors. 'trace_metadata'
    # metric is relatively unlikely to be removed from Perfetto, but if it
    # is, we will have to pick a different metric.
    run_tbmv3_metric.Main([
        '--trace', self.trace_path,
        '--metric', 'trace_metadata',
        '--outfile', self.outfile_path,
    ])
