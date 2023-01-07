#!/usr/bin/env python3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Finds all the annotated tests from proguard dump"""

import argparse
import datetime
import json
import linecache
import logging
import os
import pprint
import re
import sys
import time

_SRC_DIR = os.path.abspath(os.path.join(
      os.path.dirname(__file__), '..', '..'))

sys.path.append(os.path.join(_SRC_DIR, 'third_party', 'catapult', 'devil'))
from devil.utils import cmd_helper

sys.path.append(os.path.join(_SRC_DIR, 'build', 'android'))
from pylib import constants
from pylib.instrumentation import instrumentation_test_instance


_CRBUG_ID_PATTERN = re.compile(r'crbug(?:.com)?/(\d+)')
_EXPORT_TIME_FORMAT = '%Y%m%dT%H%M%S'
_GIT_LOG_TIME_PATTERN = re.compile(r'\d+')
_GIT_LOG_MESSAGE_PATTERN = (
    r'Cr-Commit-Position: refs/heads/(?:master|main)@{#(\d+)}')
_GIT_TIME_FORMAT = '%Y-%m-%dT%H:%M:%S'


def _GetBugId(test_annotations):
  """Find and return the test bug id from its annoation message elements"""
  # TODO(yolandyan): currently the script only supports on bug id per method,
  # add support for multiple bug id
  for content in test_annotations.items():
    if content and content.get('message'):
      search_result = re.search(_CRBUG_ID_PATTERN, content.get('message'))
      if search_result is not None:
        return int(search_result.group(1))
  return None


def _GetTests(test_apks, apk_output_dir):
  """Return a list of all annotated tests and total test count"""
  result = []
  total_test_count = 0
  for test_apk in test_apks:
    logging.info('Current test apk: %s', test_apk)
    test_jar = os.path.join(
        apk_output_dir, constants.SDK_BUILD_TEST_JAVALIB_DIR,
        '%s.jar' % test_apk)
    all_tests = instrumentation_test_instance.GetAllTests(test_jar=test_jar)
    for test_class in all_tests:
      class_path = test_class['class']
      class_name = test_class['class'].split('.')[-1]

      class_annotations = test_class['annotations']
      class_bug_id = _GetBugId(class_annotations)
      for test_method in test_class['methods']:
        total_test_count += 1
        # getting annotation of each test case
        test_annotations = test_method['annotations']
        test_bug_id = _GetBugId(test_annotations)
        test_bug_id = test_bug_id if test_bug_id else class_bug_id
        test_annotations.update(class_annotations)
        # getting test method name of each test
        test_name = test_method['method']
        test_dict = {
            'bug_id': test_bug_id,
            'annotations': test_annotations,
            'test_name': test_name,
            'test_apk_name': test_apk,
            'class_name': class_name,
            'class_path': class_path
        }
        result.append(test_dict)

  logging.info('Total count of tests in all test apks: %d', total_test_count)
  return result, total_test_count


def _GetReportMeta(utc_script_runtime_string, total_test_count):
  """Returns a dictionary of the report's metadata"""
  revision = cmd_helper.GetCmdOutput(['git', 'rev-parse', 'HEAD']).strip()
  raw_string = cmd_helper.GetCmdOutput(
      ['git', 'log', '--pretty=format:%at', '--max-count=1', 'HEAD'])
  time_string_search = re.search(_GIT_LOG_TIME_PATTERN, raw_string)
  if time_string_search is None:
    raise Exception('Timestamp format incorrect, expected all digits, got %s'
                    % raw_string)

  raw_string = cmd_helper.GetCmdOutput(
      ['git', 'log', '--pretty=format:%b', '--max-count=1', 'HEAD'])
  commit_pos_search = re.search(_GIT_LOG_MESSAGE_PATTERN, raw_string)
  if commit_pos_search is None:
    raise Exception('Cr-Commit-Position is not found, potentially running with '
                    'uncommited HEAD')
  commit_pos = int(commit_pos_search.group(1))

  utc_revision_time = datetime.datetime.utcfromtimestamp(
      int(time_string_search.group(0)))
  utc_revision_time = utc_revision_time.strftime(_EXPORT_TIME_FORMAT)
  logging.info(
      'revision is %s, revision time is %s', revision, utc_revision_time)

  return {
      'revision': revision,
      'commit_pos': commit_pos,
      'script_runtime': utc_script_runtime_string,
      'revision_time': utc_revision_time,
      'platform': 'android',
      'total_test_count': total_test_count
  }


def _GetReport(test_apks, script_runtime_string, apk_output_dir):
  """Generate the dictionary of report data

  Args:
    test_apks: a list of apks for search for tests
    script_runtime_string: the time when the script is run at
                           format: '%Y%m%dT%H%M%S'
  """

  test_data, total_test_count = _GetTests(test_apks, apk_output_dir)
  report_meta = _GetReportMeta(script_runtime_string, total_test_count)
  report_data = {
      'metadata': report_meta,
      'tests': test_data
  }
  return report_data


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('-t', '--test-apks', nargs='+', dest='test_apks',
                      required=True,
                      help='List all test apks file name that the script uses '
                           'to fetch tracked tests from')
  parser.add_argument('--json-output-dir', required=True,
                      help='JSON file output dir')
  parser.add_argument('--apk-output-dir', required=True,
                      help='The output directory of test apks')
  parser.add_argument('--timestamp-string',
                      help='The time when this script is run, passed in by the '
                           'recipe that runs this script so both the recipe '
                           'and this script use it to format output json name')
  parser.add_argument('-v', '--verbose', action='store_true', default=False,
                      help='INFO verbosity')

  arguments = parser.parse_args(sys.argv[1:])
  logging.basicConfig(
      level=logging.INFO if arguments.verbose else logging.WARNING)

  if arguments.timestamp_string is None:
    script_runtime = datetime.datetime.utcnow()
    script_runtime_string = script_runtime.strftime(_EXPORT_TIME_FORMAT)
  else:
    script_runtime_string = arguments.timestamp_string
  logging.info('Build time is %s', script_runtime_string)
  apk_output_dir = os.path.abspath(os.path.join(
      constants.DIR_SOURCE_ROOT, arguments.apk_output_dir))
  report_data = _GetReport(
      arguments.test_apks, script_runtime_string, apk_output_dir)

  json_output_path = os.path.join(
      arguments.json_output_dir,
      '%s-android-chrome.json' % script_runtime_string)
  with open(json_output_path, 'w') as f:
    json.dump(report_data, f, sort_keys=True, separators=(',',': '))
    logging.info('Saved json output file to %s', json_output_path)


if __name__ == '__main__':
  sys.exit(main())
