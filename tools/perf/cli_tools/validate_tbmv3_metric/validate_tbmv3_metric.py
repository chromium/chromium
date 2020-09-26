# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import logging
import os
import sys
import tempfile

from core.tbmv3 import run_tbmv3_metric
from py_utils import cloud_storage
from tracing.metrics import metric_runner


def SetUpLogging(level):
  logger = logging.getLogger()
  logger.setLevel(level)
  formatter = logging.Formatter(
      '(%(levelname)s) %(asctime)s [%(module)s] %(message)s')

  handler = logging.StreamHandler()
  handler.setFormatter(formatter)
  logger.addHandler(handler)


def ParseArgs():
  parser = argparse.ArgumentParser()
  parser.add_argument('--tbmv3-name',
                      type=str,
                      required=True,
                      help='TBMv3 metric name')
  parser.add_argument('--tbmv3-histogram',
                      type=str,
                      required=True,
                      help='TBMv3 histogram name')
  parser.add_argument('--tbmv2-name',
                      type=str,
                      required=True,
                      help='TBMv2 metric name')
  parser.add_argument('--tbmv2-histogram',
                      type=str,
                      required=True,
                      help='TBMv2 histogram name')
  parser.add_argument('--traces-dir',
                      type=str,
                      required=False,
                      default=os.path.join(os.getcwd(),
                                           'tbmv3-validator-traces'),
                      help=('Directory to store all intermediary files. '
                            'If non is given, ${pwd}/tbmv3-validator-traces/ '
                            'will be used'))
  # TODO(crbug.com/1128919): Remove --trace-links-path flag when downloading of
  # trace links is automated.
  parser.add_argument('--trace-links-path',
                      type=str,
                      required=False,
                      default=None,
                      help=('Path to a json file containing links to HTML '
                            'traces in CloudStorage in chrome-telemetry-output '
                            'bucket. '
                            'Default: {--traces-dir}/trace_links.json'))
  parser.add_argument('--trace-processor-path',
                      type=str,
                      required=False,
                      default=None,
                      help=('Path to trace_processor shell. '
                            'Default: Binary downloaded from cloud storage.'))
  args = parser.parse_args()
  if args.trace_links_path is None:
    args.trace_links_path = os.path.join(args.traces_dir, 'trace_links.json')
  return args


def CreateTraceFile(trace_link_prefix, traces_dir, extension):
  trace_link = '%s.%s' % (trace_link_prefix, extension)
  with tempfile.NamedTemporaryFile(dir=traces_dir,
                                   suffix='_trace.%s' % extension,
                                   delete=False) as trace_file:
    cloud_storage.Get(cloud_storage.TELEMETRY_OUTPUT, trace_link,
                      trace_file.name)
    logging.debug('Downloading trace to %s\ntrace_link: %s' %
                  (trace_file.name, trace_link))
    return trace_file.name


def GetTraces(trace_links_path, traces_dir):
  with open(trace_links_path) as html_trace_links_file:
    html_trace_links = json.load(html_trace_links_file)
  html_traces = []
  proto_traces = []
  for html_trace_link in html_trace_links:
    assert html_trace_link.endswith('.html')
    trace_link_prefix = os.path.splitext(html_trace_link)[0]
    html_traces.append(CreateTraceFile(trace_link_prefix, traces_dir, 'html'))
    # TODO(crbug.com/1128919): Fix proto file path for cases where finding the
    # proto trace requires looking into traceEvents/ directory.
    proto_traces.append(CreateTraceFile(trace_link_prefix, traces_dir, 'pb'))
  return html_traces, proto_traces


def RunTBMv2Metric(tbmv2_name, html_trace_filename, traces_dir):
  metrics = [tbmv2_name]
  TEN_MINUTES = 60 * 10
  mre_result = metric_runner.RunMetricOnSingleTrace(html_trace_filename,
                                                    metrics,
                                                    timeout=TEN_MINUTES)
  with tempfile.NamedTemporaryFile(dir=traces_dir,
                                   suffix='_tbmv2.json',
                                   delete=False) as out_file:
    json.dump(mre_result.pairs.get('histograms', []),
              out_file,
              indent=2,
              sort_keys=True,
              separators=(',', ': '))
  logging.debug('Saved TBMv2 metric to %s' % out_file.name)
  return out_file.name


def RunTBMv3Metric(tbmv3_name, proto_trace_filename, traces_dir,
                   trace_processor_path):
  with tempfile.NamedTemporaryFile(dir=traces_dir,
                                   suffix='_tbmv3.json',
                                   delete=False) as out_file:
    pass
    # Open temp file and close it so it's written to disk.
  run_tbmv3_metric.Main([
      '--trace',
      proto_trace_filename,
      '--metric',
      tbmv3_name,
      '--outfile',
      out_file.name,
      '--trace-processor-path',
      trace_processor_path,
  ])
  logging.debug('Saved TBMv3 metric to %s' % out_file.name)
  return out_file.name


def GetSortedSampleValuesFromJson(histogram_name, json_result_filename):
  with open(json_result_filename) as json_result_file:
    result = json.load(json_result_file)
  sample_values = []
  for item in result:
    if 'name' in item and item['name'] == histogram_name:
      sample_values = item['sampleValues']
  return sorted(sample_values)


def CalculateTBMv3Metric(tbmv3_name, tbmv3_histogram, tbmv3_json_filename):
  # TODO(crbug.com/1128919): Add conversion of sample values based on their
  # units.
  return GetSortedSampleValuesFromJson('%s::%s' % (tbmv3_name, tbmv3_histogram),
                                       tbmv3_json_filename)


def CalculateTBMv2Metric(tbmv2_histogram, tbmv2_json_filename):
  # TODO(crbug.com/1128919): Add conversion of sample values based on their
  # units.
  return GetSortedSampleValuesFromJson(tbmv2_histogram, tbmv2_json_filename)


def ValidateTBMv3Metric(args):
  html_traces, proto_traces = GetTraces(args.trace_links_path, args.traces_dir)

  debug_filenames_for_failed_comparisons = []
  for html_trace, proto_trace in zip(html_traces, proto_traces):
    tbmv3_out_filename = RunTBMv3Metric(args.tbmv3_name, proto_trace,
                                        args.traces_dir,
                                        args.trace_processor_path)
    tbmv3_metric = CalculateTBMv3Metric(args.tbmv3_name, args.tbmv3_histogram,
                                        tbmv3_out_filename)
    tbmv2_out_filename = RunTBMv2Metric(args.tbmv2_name, html_trace,
                                        args.traces_dir)
    tbmv2_metric = CalculateTBMv2Metric(args.tbmv2_histogram,
                                        tbmv2_out_filename)
    if tbmv3_metric != tbmv2_metric:
      logging.warning('TBMv3 differs from TBMv2 for trace %s' % html_trace)
      debug_filenames_for_failed_comparisons.append(
          (tbmv3_out_filename, tbmv2_out_filename, html_trace, proto_trace))
  if len(debug_filenames_for_failed_comparisons) == 0:
    print 'SUCCESS!'
    return 0
  print 'TBMv3 validation failed for traces:'
  for filenames in debug_filenames_for_failed_comparisons:
    print(('\ttbmv3 json: %s\n\ttbmv2 json: %s\n\thtml trace: %s\n'
           '\tproto trace: %s\n') % filenames)
  return 1


def Main():
  SetUpLogging(level=logging.WARNING)
  args = ParseArgs()
  sys.exit(ValidateTBMv3Metric(args))
