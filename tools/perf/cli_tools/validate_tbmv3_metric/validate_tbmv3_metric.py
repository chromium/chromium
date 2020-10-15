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
from py_utils import cloud_storage
from tracing.metrics import metric_runner


_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


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
                      default=('/'.join([_SCRIPT_DIR, 'traces'])),
                      help=('Directory to store all intermediary files. '
                            'If non is given, %s will be used') %
                      '/'.join([_SCRIPT_DIR, 'traces']))
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
  args = parser.parse_args()
  if args.trace_links_csv_path is None:
    args.trace_links_csv_path = '/'.join([args.traces_dir, 'trace_links.csv'])
  return args


def DownloadTraceFile(trace_link, traces_dir):
  trace_link_extension = os.path.splitext(trace_link)[1]
  if trace_link.startswith('/'):
    trace_link = trace_link[1:]
  if not os.path.exists(traces_dir):
    os.mkdir(traces_dir, 0755)
  with tempfile.NamedTemporaryFile(dir=traces_dir,
                                   suffix='_trace%s' % trace_link_extension,
                                   delete=False) as trace_file:
    cloud_storage.Get(cloud_storage.TELEMETRY_OUTPUT, trace_link,
                      trace_file.name)
    logging.debug('Downloading trace to %s\ntrace_link: %s.' %
                  (trace_file.name, trace_link))
    return trace_file.name


def GetProtoTraceLinkFromTraceEventsDir(link_prefix):
  proto_link_prefix = '/'.join([link_prefix, 'trace/traceEvents/**'])
  proto_link = None
  try:
    for link in cloud_storage.List(cloud_storage.TELEMETRY_OUTPUT,
                                   proto_link_prefix):
      if link.endswith('.pb.gz') or link.endswith('.pb'):
        proto_link = link
        break
    if proto_link is not None:
      return proto_link
    raise cloud_storage.NotFoundError(
        'Proto trace link not found in cloud storage. Path: %s.' %
        proto_link_prefix)
  except cloud_storage.NotFoundError, e:
    raise cloud_storage.NotFoundError('No URLs match the prefix %s: %s' %
                                      (proto_link_prefix, str(e)))


def ParseGSLinksFromHTTPLink(http_link):
  """Parses gs:// links to traces from HTTP link.

  The link to HTML trace can be obtained by substituting the part of http_link
  ending with /o/ with 'gs://chrome-telemetry-output/'.

  The link to proto trace in the simplest case can be obtained from HTML trace
  link by replacing the extension from 'html' to 'pb'. In case this approach
  does not work the proto trace link can be found in trace/traceEvents
  subdirectory.
  For example, the first approach works for
  https://console.developers.google.com/m/cloudstorage/b/chrome-telemetry-output/o/20201004T094119_6100/rendering.desktop/animometer_webgl_attrib_arrays/retry_0/trace.html:
  The cloud storage paths to HTML and proto traces are:
  20201004T094119_6100/rendering.desktop/animometer_webgl_attrib_arrays/retry_0/trace.html
  20201004T094119_6100/rendering.desktop/animometer_webgl_attrib_arrays/retry_0/trace.pb,
  but doesn't work for
  https://console.developers.google.com/m/cloudstorage/b/chrome-telemetry-output/o/20200928T183503_42028/v8.browsing_desktop/browse_social_tumblr_infinite_scroll_2018/retry_0/trace.html:
  The cloud storage paths to HTML and proto traces are:
  20200928T183503_42028/v8.browsing_desktop/browse_social_tumblr_infinite_scroll_2018/
  retry_0/trace.html,
  20200928T183503_42028/v8.browsing_desktop/browse_social_tumblr_infinite_scroll_2018/
  retry_0/trace/traceEvents/tmpTq5XNv.pb.gz
  """
  html_link_suffix = '/trace.html'
  assert http_link.endswith(html_link_suffix), (
      'Link passed to ParseGSLinksFromHTTPLink ("%s") is '
      ' invalid. The link must end with "%s".') % (http_link, html_link_suffix)

  html_link = http_link.split('/o/')[1]
  if not cloud_storage.Exists(cloud_storage.TELEMETRY_OUTPUT, html_link):
    raise cloud_storage.NotFoundError(
        'HTML trace link %s not found in cloud storage.' % html_link)

  proto_link = os.path.splitext(html_link)[0] + '.pb'

  if not cloud_storage.Exists(cloud_storage.TELEMETRY_OUTPUT, proto_link):
    link_prefix = html_link[:-len(html_link_suffix)]
    proto_link = GetProtoTraceLinkFromTraceEventsDir(link_prefix)
  return html_link, proto_link


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
    html_link, proto_link = ParseGSLinksFromHTTPLink(trace_info['Trace Link'])
    html_trace = DownloadTraceFile(html_link, args.traces_dir)
    proto_trace = DownloadTraceFile(proto_link, args.traces_dir)
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
  SetUpLogging(level=logging.WARNING)
  args = ParseArgs()
  sys.exit(ValidateTBMv3Metric(args))
