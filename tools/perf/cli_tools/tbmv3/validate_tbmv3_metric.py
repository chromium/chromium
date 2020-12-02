# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import logging
import os
import sys
import tempfile
import csv

from core.tbmv3 import run_tbmv3_metric
from cli_tools.tbmv3 import trace_downloader
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
                      default=trace_downloader.DEFAULT_TRACE_DIR,
                      help='Directory to store all intermediate files')
  parser.add_argument('--trace-links-csv-path',
                      type=str,
                      required=False,
                      default=None,
                      help=('Path to a csv file containing links to HTML '
                            'traces in CloudStorage in chrome-telemetry-output '
                            'bucket. Go to go/get-tbm-traces and follow '
                            'instructions there to generate the CSV. '
                            'Default: {--traces-dir}/trace_links.csv'))
  parser.add_argument('--trace-processor-path',
                      type=str,
                      required=False,
                      default=None,
                      help=('Path to trace_processor shell. '
                            'Default: Binary downloaded from cloud storage.'))
  parser.add_argument('-v', '--verbose', action='store_true')
  args = parser.parse_args()
  if args.trace_links_csv_path is None:
    args.trace_links_csv_path = '/'.join([args.traces_dir, 'trace_links.csv'])
  return args


def RunTBMv2Metric(tbmv2_name, html_trace_filename, traces_dir):
  metrics = [tbmv2_name]
  TEN_MINUTES = 60 * 10
  trace_abspath = os.path.abspath(html_trace_filename)
  mre_result = metric_runner.RunMetricOnSingleTrace(trace_abspath,
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


def CalculateTBMv3Metric(tbmv3_histogram, tbmv3_json_filename):
  # TODO(crbug.com/1128919): Add conversion of sample values based on their
  # units.
  return GetSortedSampleValuesFromJson(tbmv3_histogram, tbmv3_json_filename)


def CalculateTBMv2Metric(tbmv2_histogram, tbmv2_json_filename):
  # TODO(crbug.com/1128919): Add conversion of sample values based on their
  # units.
  return GetSortedSampleValuesFromJson(tbmv2_histogram, tbmv2_json_filename)


def ValidateTBMv3Metric(args):
  reader = csv.DictReader(open(args.trace_links_csv_path))
  debug_info_for_failed_comparisons = []

  for trace_info in reader:
    bot = trace_info['Bot']
    benchmark = trace_info['Benchmark']
    html_trace_url = trace_info['Trace Link']
    html_trace = trace_downloader.DownloadHtmlTrace(
        html_trace_url, download_dir=args.traces_dir)
    proto_trace = trace_downloader.DownloadProtoTrace(
        html_trace_url, download_dir=args.traces_dir)
    tbmv3_out_filename = RunTBMv3Metric(args.tbmv3_name, proto_trace,
                                        args.traces_dir,
                                        args.trace_processor_path)
    tbmv3_metric = CalculateTBMv3Metric(args.tbmv3_histogram,
                                        tbmv3_out_filename)
    tbmv2_out_filename = RunTBMv2Metric(args.tbmv2_name, html_trace,
                                        args.traces_dir)
    tbmv2_metric = CalculateTBMv2Metric(args.tbmv2_histogram,
                                        tbmv2_out_filename)
    if len(tbmv2_metric) == 0:
      logging.warning('TBMv2 metric is empty for bot: %s, benchmark: %s' %
                      (bot, benchmark))
    if len(tbmv3_metric) == 0:
      logging.warning('TBMv3 metric is empty for bot: %s, benchmark: %s' %
                      (bot, benchmark))
    if tbmv3_metric != tbmv2_metric:
      logging.warning('TBMv3 differs from TBMv2 for bot: %s, benchmark: %s' %
                      (bot, benchmark))
      debug_info_for_failed_comparisons.append({
          'tbmv3 json': tbmv3_out_filename,
          'tbmv2 json': tbmv2_out_filename,
          'html trace': html_trace,
          'proto trace': proto_trace,
          'bot': bot,
          'benchmark': benchmark
      })
  if len(debug_info_for_failed_comparisons) == 0:
    print 'SUCCESS!'
    return 0
  print 'TBMv3 validation failed for traces:'
  for filenames in debug_info_for_failed_comparisons:
    print(('\tBot: {bot}\n\tBenchmark: {benchmark}\n'
           '\ttbmv3 json: {tbmv3 json}\n\ttbmv2 json: {tbmv2 json}\n'
           '\thtml trace: {html trace}\n\tproto trace: {proto trace}\n').format(
               **filenames))
  return 1


def Main():
  args = ParseArgs()
  loglevel = logging.DEBUG if args.verbose else logging.WARNING
  SetUpLogging(level=loglevel)
  sys.exit(ValidateTBMv3Metric(args))
