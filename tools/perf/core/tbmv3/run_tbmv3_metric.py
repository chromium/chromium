#!/usr/bin/env vpython
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import argparse
import json
import os
import subprocess
import sys

from collections import namedtuple

# TODO(crbug.com/1012687): Adding tools/perf to path. We can remove this when
# we have a wrapper script under tools/perf that sets up import paths more
# nicely.
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))

from core import path_util
path_util.AddPyUtilsToPath()
path_util.AddTracingToPath()

from tracing.value import histogram_set

_CHROMIUM_SRC_PATH  = os.path.join(
    os.path.dirname(__file__), '..', '..', '..', '..')
_DEFAULT_TP_PATH = os.path.realpath(os.path.join(
    _CHROMIUM_SRC_PATH, 'out', 'Debug', 'trace_processor_shell'))
_METRICS_PATH = os.path.realpath(os.path.join(os.path.dirname(__file__),
                                              'metrics'))

MetricFiles = namedtuple('MetricFiles', ('sql', 'proto', 'config'))


def CreateMetricFiles(metric_name):
  return MetricFiles(
    sql=os.path.join(_METRICS_PATH, metric_name + '.sql'),
    proto=os.path.join(_METRICS_PATH, metric_name + '.proto'),
    config=os.path.join(_METRICS_PATH, metric_name + '_config.json'))


class MetricFileNotFound(Exception):
  pass


class TraceProcessorNotFound(Exception):
  pass


class TraceProcessorError(Exception):
  pass


def _CheckFilesExist(trace_processor_path, metric_files):
  if not os.path.exists(trace_processor_path):
    raise TraceProcessorNotFound('Could not find trace processor shell at %s'
                                 % trace_processor_path)

  # Currently assuming all metric files live in tbmv3/metrics directory. We will
  # revise this decision later.
  for filetype, path in metric_files._asdict().iteritems():
    if not os.path.exists(path):
      raise MetricFileNotFound('metric %s file not found at %s'
                               % (filetype, path))


def _RunTraceProcessorMetric(trace_processor_path, trace, metric_files):
  trace_processor_args = [trace_processor_path, trace,
                          '--run-metrics', metric_files.sql,
                          '--metrics-output=json',
                          '--extra-metrics', _METRICS_PATH]

  try:
    output = subprocess.check_output(trace_processor_args)
  except subprocess.CalledProcessError:
    raise TraceProcessorError(
        'Failed to compute metrics. Check trace processor logs.')
  return json.loads(output)


def _ScopedHistogramName(metric_name, histogram_name):
  """Returns scoped histogram name by preprending metric name.

  This is useful for avoiding histogram name collision. The '_metric' suffix of
  the metric name is dropped from scoped name. Example:
  _ScopedHistogramName("console_error_metric", "js_errors")
  => "console_error::js_errors"
  """
  metric_suffix = '_metric'
  suffix_length = len(metric_suffix)
  # TODO(crbug.com/1012687): Decide on whether metrics should always have
  # '_metric' suffix.
  if metric_name[-suffix_length:] == metric_suffix:
    scope = metric_name[:-suffix_length]
  else:
    scope = metric_name
  return '::'.join([scope, histogram_name])


def _ProduceHistograms(metric_name, metric_files, measurements):
  histograms = histogram_set.HistogramSet()
  with open(metric_files.config) as f:
    config = json.load(f)
  metric_root_field = 'perfetto.protos.' + metric_name
  for histogram_config in config['histograms']:
    histogram_name = histogram_config['name']
    samples = measurements[metric_root_field][histogram_name]
    scoped_histogram_name = _ScopedHistogramName(metric_name, histogram_name)
    description = histogram_config['description']
    histograms.CreateHistogram(scoped_histogram_name,
                               histogram_config['unit'], samples,
                               description=description)
  return histograms


def _WriteHistogramSetToFile(histograms, outfile):
  with open(outfile, 'w') as f:
    json.dump(histograms.AsDicts(), f, indent=2, sort_keys=True,
              separators=(',', ': '))


def Main():
  parser = argparse.ArgumentParser(
    description='[Experimental] Runs TBMv3 metrics on local traces and '
    'produces histogram json.')
  parser.add_argument('--trace', required=True,
                      help='Trace file you want to compute metric on')
  parser.add_argument('--metric', required=True,
                      help=('Name of the metric you want to run'))
  parser.add_argument('--trace_processor_path', default=_DEFAULT_TP_PATH,
                      help='Path to trace processor shell. '
                      'Default: %(default)s')
  parser.add_argument('--outfile', default='results.json',
                      help='Path to output file. Default: %(default)s')
  args = parser.parse_args()

  metric_files = CreateMetricFiles(args.metric)
  _CheckFilesExist(args.trace_processor_path, metric_files)

  measurements = _RunTraceProcessorMetric(args.trace_processor_path,
                                               args.trace, metric_files)
  histograms = _ProduceHistograms(args.metric, metric_files, measurements)
  _WriteHistogramSetToFile(histograms, args.outfile)
  print('JSON result created in file://%s' % (args.outfile))


if __name__ == '__main__':
  sys.exit(Main())
