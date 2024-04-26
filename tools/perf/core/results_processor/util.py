# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import calendar
import datetime
import json
import logging
import os

import requests  # pylint: disable=import-error

import multiprocessing
from multiprocessing.dummy import Pool as ThreadPool

import sys

TELEMETRY_TEST_PATH_FORMAT = 'telemetry'
GTEST_TEST_PATH_FORMAT = 'gtest'


def ApplyInParallel(function, work_list, on_failure=None):
  """Apply a function to all values in work_list in parallel.

  Args:
    function: A function with one argument.
    work_list: Any iterable with arguments for the function.
    on_failure: A function to run in case of a failure.
  """
  if not work_list:
    return

  try:
    # Note that this is speculatively halved as an attempt to fix
    # crbug.com/953365.
    cpu_count = multiprocessing.cpu_count() // 2
    if sys.platform == 'win32':
      # TODO(crbug.com/40755900) - we can't use more than 56
      # cores on Windows or Python3 may hang.
      cpu_count = min(cpu_count, 56)

  except NotImplementedError:
    # Some platforms can raise a NotImplementedError from cpu_count()
    logging.warning('cpu_count() not implemented.')
    cpu_count = 4
  pool = ThreadPool(min(cpu_count, len(work_list)))

  def function_with_try(arg):
    try:
      function(arg)
    except Exception:  # pylint: disable=broad-except
      # logging exception here is the only way to get a stack trace since
      # multiprocessing's pool implementation does not save that data. See
      # crbug.com/953365.
      logging.exception('Exception while running %s' % function.__name__)
      if on_failure:
        on_failure(arg)

  try:
    pool.imap_unordered(function_with_try, work_list)
    pool.close()
    pool.join()
  finally:
    pool.terminate()


def SplitTestPath(test_result, test_path_format):
  """ Split a test path into test suite name and test case name.

  Telemetry and Gtest have slightly different test path formats.
  Telemetry uses '{benchmark_name}/{story_name}', e.g.
  'system_health.common_desktop/load:news:cnn:2020'.
  Gtest uses '{test_suite_name}.{test_case_name}', e.g.
  'ZeroToFiveSequence/LuciTestResultParameterizedTest.Variant'
  """
  if test_path_format == TELEMETRY_TEST_PATH_FORMAT:
    separator = '/'
  elif test_path_format == GTEST_TEST_PATH_FORMAT:
    separator = '.'
  else:
    raise ValueError('Unknown test path format: %s' % test_path_format)

  test_path = test_result['testPath']
  if separator not in test_path:
    raise ValueError('Invalid test path: %s' % test_path)

  return test_path.split(separator, 1)


def IsoTimestampToEpoch(timestamp):
  """Convert ISO formatted time to seconds since epoch."""
  try:
    dt = datetime.datetime.strptime(timestamp, '%Y-%m-%dT%H:%M:%S.%fZ')
  except ValueError:
    dt = datetime.datetime.strptime(timestamp, '%Y-%m-%dT%H:%M:%SZ')
  return calendar.timegm(dt.timetuple()) + dt.microsecond / 1e6


def SetUnexpectedFailure(test_result):
  """Update fields of a test result in a case of processing failure."""
  test_result['status'] = 'FAIL'
  test_result['expected'] = False
  logging.error('Processing failed for test %s', test_result['testPath'])


def TryUploadingResultToResultSink(results):
  def buildSummaryHtml(artifacts):
    # Using test log as the summary. It is stored in an artifact named logs.txt.
    if 'logs.txt' in artifacts:
      summary_html = '<p><text-artifact artifact-id="logs.txt"></p>'
    else:
      summary_html = ''
    return summary_html

  def buildArtifacts(artifacts):
    artifacts_result = {}
    for artifact_id, artifact in artifacts.items():
      artifacts_result[artifact_id] = {'filePath': artifact['filePath']}
    return artifacts_result

  def parse(results):
    test_results = []
    for test_case in results:
      test_result = {
          'testId': test_case['testPath'],
          'expected': test_case['expected'],
          'status': test_case['status']
      }
      # TODO: go/result-sink#test-result-json-object listed that specifying
      # testMetadata with location info can helped with breaking down flaky
      # tests. We don't have the file location currently in test results.
      if 'runDuration' in test_case:
        test_result['duration'] = '%.9fs' % float(
            test_case['runDuration'].rstrip('s'))
      if 'tags' in test_case:
        test_result['tags'] = test_case['tags']
      if 'outputArtifacts' in test_case:
        test_result['summaryHtml'] = buildSummaryHtml(
            test_case['outputArtifacts'])
        test_result['artifacts'] = buildArtifacts(test_case['outputArtifacts'])
      test_results.append(test_result)
    return test_results

  try:
    with open(os.environ['LUCI_CONTEXT']) as f:
      sink = json.load(f)['result_sink']
  except KeyError:
    return

  test_results = parse(results)
  res = requests.post(
      url='http://%s/prpc/luci.resultsink.v1.Sink/ReportTestResults' %
      sink['address'],
      headers={
          'Content-Type': 'application/json',
          'Accept': 'application/json',
          'Authorization': 'ResultSink %s' % sink['auth_token'],
      },
      data=json.dumps({'testResults': test_results}))
  res.raise_for_status()
