# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import calendar
import datetime
import logging
import multiprocessing
from multiprocessing.dummy import Pool as ThreadPool


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
    cpu_count = multiprocessing.cpu_count() / 2
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
