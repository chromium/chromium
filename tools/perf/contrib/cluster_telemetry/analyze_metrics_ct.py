#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import division
from __future__ import print_function

import argparse
import json
import logging
import os
import sys
import tempfile
import time


sys.path.insert(1, os.path.join(os.path.dirname(__file__),
                                os.pardir, os.pardir, os.pardir, os.pardir,
                                'third_party', 'catapult', 'tracing'))
from tracing.metrics import metric_runner
from tracing.metrics import discover
from tracing.value import histograms_to_csv


def main():
  all_metrics = discover.DiscoverMetrics(
      ['/tracing/metrics/all_metrics.html'])

  parser = argparse.ArgumentParser(
      description='Runs metrics on a local trace')
  parser.add_argument('--local-trace-path', type=str,
                      help='The local path to the trace file')
  parser.add_argument('--cloud-trace-link', type=str,
                      help=('Cloud link from where the local trace file was '
                            'downloaded from'))
  parser.add_argument('--metric-name', type=str,
                      help=('Function name of registered metric '
                            '(not filename.) Available metrics are: %s' %
                            ', '.join(all_metrics)))
  parser.add_argument('--output-csv', default='results', type=str,
                      help='Output CSV file path')

  args = parser.parse_args()

  trace_size_in_mib = os.path.getsize(args.local_trace_path) / (2 ** 20)
  # Bails out on trace that are too big. See crbug.com/812631 for more details.
  if trace_size_in_mib > 400:
    print('Trace size is too big: %s MiB' % trace_size_in_mib)
    return 1

  logging.warning('Starting to compute metrics on trace')
  start = time.time()
  mre_result = metric_runner.RunMetric(
      args.local_trace_path, [args.metric_name], {},
      report_progress=False,
      canonical_url=args.cloud_trace_link)
  logging.warning('Processing resulting traces took %.3f seconds' % (
      time.time() - start))

  for f in mre_result.failures:
    print('Running metric failed:')
    print(f.stack)
    return 1

  with tempfile.NamedTemporaryFile(mode='w') as temp:
    json.dump(mre_result.pairs.get('histograms', []), temp, indent=2,
              sort_keys=True, separators=(',', ': '))
    temp.flush()

    result = histograms_to_csv.HistogramsToCsv(temp.name)
    if result.returncode != 0:
      print('histograms_to_csv.HistogramsToCsv returned %d' % result.returncode)
      return result.returncode
    with open(args.output_csv, 'wb') as f:
      f.write(result.stdout.rstrip())
    print('Output CSV created in file://' + args.output_csv)


if __name__ == '__main__':
  sys.exit(main())
