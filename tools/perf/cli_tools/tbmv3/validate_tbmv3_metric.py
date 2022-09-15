# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import ast
import argparse
import importlib
import json
import logging
import os
import pprint
import sys
import unittest
import csv

from collections import namedtuple

from core.tbmv3 import trace_processor
from cli_tools.tbmv3 import trace_downloader
from tracing.metrics import metric_runner
from tracing.value import histogram_set


SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
SIMPLE_CONFIG_PATH = os.path.join(SCRIPT_DIR, 'validators',
                                  'simple_configs.pyl')

SimpleConfig = namedtuple('SimpleConfig', ['name', 'config'])


def SetUpLogging(level):
  logger = logging.getLogger()
  logger.setLevel(level)
  formatter = logging.Formatter(
      '(%(levelname)s) %(asctime)s [%(module)s] %(message)s')

  handler = logging.StreamHandler()
  handler.setFormatter(formatter)
  logger.addHandler(handler)


def PrintNoLn(msg):
  """Print |msg| without adding new line."""
  sys.stdout.write(msg)
  sys.stdout.flush()


def CursorErase(length):
  """Erase |length| chars starting from cursor."""
  for _ in range(length):
    sys.stdout.write('\b')
  # Add 80 spaces, because \b only moves back the cursor.
  for _ in range(80):
    sys.stdout.write(' ')
  for _ in range(80):
    sys.stdout.write('\b')
  sys.stdout.flush()


def ParseArgs():
  parser = argparse.ArgumentParser()
  parser.add_argument('validator',
                      type=str,
                      default=None,
                      help=('Name of the validtor from tools/perf/'
                            'cli_tools/tbmv3/validators/, or alternatively '
                            'a simple config defined in cli_tools/tbmv3/'
                            'validators/simple_configs.pyl'))
  parser.add_argument('--tracelist-csv',
                      type=str,
                      required=False,
                      default=None,
                      help=('Path to a csv file containing links to HTML '
                            'traces in CloudStorage in chrome-telemetry-output '
                            'bucket. Go to go/get-tbm-traces and follow '
                            'instructions there to generate the CSV.'))
  parser.add_argument('--proto-trace',
                      type=str,
                      required=False,
                      help='Path to proto trace')
  parser.add_argument('--json-trace',
                      type=str,
                      required=False,
                      help='Path to json/html trace')
  parser.add_argument('--traces-dir',
                      type=str,
                      required=False,
                      default=trace_downloader.DEFAULT_TRACE_DIR,
                      help='Directory to store all intermediate files')
  parser.add_argument('--trace-processor-path',
                      type=str,
                      required=False,
                      default=None,
                      help=('Path to trace_processor shell. '
                            'Default: Binary downloaded from cloud storage.'))
  parser.add_argument('--force-recompute-tbmv2',
                      action='store_true',
                      help=('Recompute TBMv2 Metrics. Otherwise it will use '
                            'a cached result when available.'))
  parser.add_argument('-v', '--verbose', action='store_true')
  args = parser.parse_args()
  return args


class ValidatorContext(object):
  def __init__(self, args):
    with open(SIMPLE_CONFIG_PATH) as f:
      simple_configs = ast.literal_eval(f.read())
    validator_name = args.validator
    if validator_name in simple_configs:
      self.validator = importlib.import_module('cli_tools.tbmv3.validators.'
                                               'simple_validator')
      self.simple_config = SimpleConfig(validator_name,
                                        simple_configs[validator_name])
    else:
      self.validator = importlib.import_module('cli_tools.tbmv3.validators.' +
                                               args.validator)
      self.simple_config = None

    self.trace_processor_path = args.trace_processor_path
    if self.trace_processor_path and not os.path.exists(
        self.trace_processor_path):
      raise Exception("Trace processor does not exist at %s" %
                      args.trace_processor_path)

    self.traces_dir = args.traces_dir
    self.force_recompute_tbmv2 = args.force_recompute_tbmv2


class TraceInfo(object):
  def __init__(self, json_trace, proto_trace):
    self.json_trace = json_trace
    self.proto_trace = proto_trace
    # If present, holds additional info about trace like bot, cloud url etc.
    self.trace_metadata = None

  def __repr__(self):
    output = {
        'json_trace_path': self.json_trace,
        'proto_trace_path': self.proto_trace,
    }
    if self.trace_metadata:
      output.update(self.trace_metadata)
    return pprint.pformat(output)


def CreateTraceInfoFromCsvRow(row, traces_dir):
  message = 'Fetching traces...'
  PrintNoLn(message)
  html_trace_url = row['Trace Link']
  html_trace = trace_downloader.DownloadHtmlTrace(html_trace_url,
                                                  download_dir=traces_dir)
  proto_trace = trace_downloader.DownloadProtoTrace(html_trace_url,
                                                    download_dir=traces_dir)

  trace_info = TraceInfo(html_trace, proto_trace)
  trace_info.trace_metadata = {
      'Bot': row['Bot'],
      'Benchmark': row['Benchmark'],
      'Cloud Trace URL': html_trace_url
  }
  CursorErase(len(message))
  return trace_info


def CreateTraceInfoFromArgs(args):
  json_trace = os.path.expanduser(args.json_trace)
  if json_trace is None:
    raise Exception('You must supply a --json_trace if you do not use '
                    '--tracelist-csv.')
  if not os.path.exists(json_trace):
    raise Exception('Json trace %s does not exist' % json_trace)

  proto_trace = os.path.expanduser(args.proto_trace)
  if proto_trace is None:
    raise Exception('You must supply a --proto_trace if you do not use '
                    '--tracelist-csv.')
  if not os.path.exists(proto_trace):
    raise Exception('Proto trace %s does not exist' % proto_trace)

  return TraceInfo(json_trace, proto_trace)


def GetV2CachedResultPath(tbmv2_metric, json_trace):
  dirname = os.path.dirname(json_trace)
  basename = os.path.basename(json_trace) + '.' + tbmv2_metric + '.json'
  return os.path.join(dirname, basename)


def RunTBMv2Metric(tbmv2_metric, json_trace, force_recompute=False):
  message = 'Running TBMv2 Metric...'
  PrintNoLn(message)
  hset = histogram_set.HistogramSet()

  cached_results = GetV2CachedResultPath(tbmv2_metric, json_trace)

  if not force_recompute and os.path.exists(cached_results):
    with open(cached_results) as f:
      hset.ImportDicts(json.load(f))
    CursorErase(len(message))
    return hset

  metrics = [tbmv2_metric]
  TEN_MINUTES = 60 * 10
  trace_abspath = os.path.abspath(json_trace)
  mre_result = metric_runner.RunMetricOnSingleTrace(trace_abspath,
                                                    metrics,
                                                    timeout=TEN_MINUTES)
  histograms = mre_result.pairs.get('histograms')
  if mre_result.failures:
    raise Exception("Error computing TBMv2 metric for %s" % json_trace)
  if 'histograms' not in mre_result.pairs:
    raise Exception("Metric %s is empty for trace %s" %
                    (tbmv2_metric, json_trace))
  histograms = mre_result.pairs['histograms']
  hset.ImportDicts(histograms)
  with open(cached_results, 'w') as f:
    json.dump(histograms, f)

  CursorErase(len(message))
  return hset


def RunTBMv3Metric(tp_path, tbmv3_metric, proto_trace):
  message = 'Running TBMv3 Metric...'
  PrintNoLn(message)
  histograms = trace_processor.RunMetric(tp_path,
                                         proto_trace,
                                         tbmv3_metric,
                                         retain_all_samples=True)
  CursorErase(len(message))
  return histograms


def ValidateSingleTrace(ctx, trace_info):
  class ValidatorTestCase(unittest.TestCase):
    def setUp(self):
      self.trace_info = trace_info
      if ctx.simple_config:
        self.simple_config = ctx.simple_config

    def RunTBMv2(self, metric):
      return RunTBMv2Metric(metric,
                            trace_info.json_trace,
                            force_recompute=ctx.force_recompute_tbmv2)

    def RunTBMv3(self, metric):
      return RunTBMv3Metric(ctx.trace_processor_path, metric,
                            trace_info.proto_trace)

    def runTest(self):
      ctx.validator.CompareHistograms(self)

  result = unittest.TestResult()
  validator_tc = ValidatorTestCase()
  validator_tc.run(result)
  return result


def ValidateAllCsvTraces(ctx, tracelist_csv, results):
  with open(os.path.expanduser(tracelist_csv)) as f:
    rows = list(csv.DictReader(f))

  for (i, row) in enumerate(rows, start=1):
    PrintNoLn('Validating trace %d of %d: ' % (i, len(rows)))
    trace_info = CreateTraceInfoFromCsvRow(row, ctx.traces_dir)
    result = ValidateSingleTrace(ctx, trace_info)
    results.append(result)
    if result.wasSuccessful():
      print('Success!')
    else:
      print('Failed.')
      PrintErrorsOrFailures(result)


def PrintSingleFailure(error_or_failure):
  trace_info = error_or_failure[0].trace_info
  error_msg = error_or_failure[1]
  print('-------------------------------------------')
  print('Validator failure for the following trace:')
  print(trace_info)
  print('Error: ')
  print(error_msg)
  print('-------------------------------------------')


def PrintErrorsOrFailures(result):
  if result.wasSuccessful():
    return
  for error in result.errors:
    PrintSingleFailure(error)
  for failure in result.failures:
    PrintSingleFailure(failure)


def CountFailures(results):
  count = 0
  for result in results:
    if not result.wasSuccessful():
      count += 1
  return count


def Main():
  args = ParseArgs()
  loglevel = logging.DEBUG if args.verbose else logging.WARNING
  SetUpLogging(level=loglevel)

  ctx = ValidatorContext(args)

  results = []
  try:
    if args.tracelist_csv:
      ValidateAllCsvTraces(ctx, args.tracelist_csv, results)
    elif args.json_trace and args.proto_trace:
      trace_info = CreateTraceInfoFromArgs(args)
      result = ValidateSingleTrace(ctx, trace_info)
      PrintErrorsOrFailures(result)
      results.append(result)
    else:
      sys.stderr.write(
          'You must supply either --tracelist_csv to validate '
          'traces, or both --proto-trace and --json-trace to validate a '
          'single trace.')
      sys.exit(1)
  except KeyboardInterrupt:
    print('\n')
    failures = CountFailures(results)
    successes = len(results) - failures
    print('%d failed, %d succeeded' % (failures, successes))
    # Without this various child processes often hangs the terminal.
    os._exit(1)  # pylint: disable=protected-access

  failures = CountFailures(results)
  if failures == 0:
    print('All validations succeeded!')
  else:
    print('%d out of %d validations did not pass. See above for details.' %
          (failures, len(results)))
