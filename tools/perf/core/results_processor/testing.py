# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper functions to build intermediate results for testing."""

import json


def TestResult(test_path, status='PASS', expected=None,
               start_time='2015-10-21T07:28:00.000Z', run_duration='1.00s',
               output_artifacts=None, tags=None, result_id=None):
  """Build a TestResult dict.

  This follows the TestResultEntry spec of LUCI Test Results format.
  See: go/luci-test-results-design

  Args:
    test_path: A string with the path that identifies this test. Usually of
      the form '{benchmark_name}/{story_name}'.
    status: An optional string indicating the status of the test run. Usually
      one of 'PASS', 'SKIP', 'FAIL'. Defaults to 'PASS'.
    expected: An optional bool indicating whether the status result is
      expected. Defaults to True for 'PASS', 'SKIP'; and False otherwise.
    start_time: An optional UTC timestamp recording when the test run started.
    run_duration: An optional duration string recording the amount of time
      that the test run lasted.
    output_artifacts: An optional mapping of artifact names to Artifact dicts,
      may be given as a dict or a sequence of pairs.
    tags: An optional sequence of tags associated with this test run; each
      tag is given as a '{key}:{value}' string. Keys are not unique, the same
      key may appear multiple times.

  Returns:
    A TestResult dict.
  """
  if expected is None:
    expected = status in ('PASS', 'SKIP')
  test_result = {
      'testPath': test_path,
      'status': status,
      'expected': expected,
      'startTime': start_time,
      'runDuration': run_duration,
  }
  if output_artifacts is not None:
    test_result['outputArtifacts'] = dict(output_artifacts)
  if tags is not None:
    test_result['tags'] = [_SplitTag(tag) for tag in tags]
  if result_id is not None:
    test_result['resultId'] = str(result_id)

  return test_result


def Artifact(file_path, view_url=None, fetch_url=None,
             content_type='application/octet-stream'):
  """Build an Artifact dict.

  Args:
    file_path: A string with the absolute path where the artifact is stored.
    view_url: An optional string with a URL where the artifact has been
      uploaded to as a human-viewable link.
    fetch_url: An optional string with a URL where the artifact has been
      uploaded to as a machine downloadable link.
    content_type: An optional string with the MIME type of the artifact.
  """
  artifact = {'filePath': file_path, 'contentType': content_type}
  if view_url is not None:
    artifact['viewUrl'] = view_url
  if fetch_url is not None:
    artifact['fetchUrl'] = fetch_url
  return artifact


def SerializeIntermediateResults(in_results, filepath):
  """Serialize intermediate results to a filepath.

  Args:
    in_results: A list of test results.
    filepath: A file path where to serialize the intermediate results.
  """
  with open(filepath, 'w') as fp:
    for test_result in in_results:
      json.dump({'testResult': test_result}, fp,
                sort_keys=True, separators=(',', ':'))
      fp.write('\n')


def _SplitTag(tag):
  key, value = tag.split(':', 1)
  return {'key': key, 'value': value}
