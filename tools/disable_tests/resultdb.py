# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Functions for querying ResultDB, via the "rdb rpc" subcommand."""

import datetime
import subprocess
import json
import re
from typing import Optional, Tuple

import errors


def get_test_metadata(invocation, test_regex: str) -> Tuple[str, str]:
  """Fetch test metadata from ResultDB.

  Args:
    invocation: the invocation to fetch the test
    test_regex: The regex to match the test id
  Returns:
    A tuple of (test name, filename). The test name will for example have the
    form  SuitName.TestName for GTest tests. The filename is the location in the
    source tree where this test is defined.
  """
  test_results = query_test_result(invocation=invocation, test_regex=test_regex)
  if 'testResults' not in test_results:
    raise errors.UserError(
        f"ResultDB couldn't query for invocation: {invocation}")

  if len(test_results["testResults"]) == 0:
    raise errors.UserError(
        f"ResultDB couldn't find test result for test regex {test_regex}")

  result = test_results["testResults"][0]
  try:
    name = result['testMetadata']['name']
    loc = result['testMetadata']['location']
    repo, filename = loc['repo'], loc['fileName']
  except KeyError as e:
    raise errors.InternalError(
        f"Malformed GetTestResult response: no key {e}") from e

  if repo != 'https://chromium.googlesource.com/chromium/src':
    raise errors.UserError(
        f"Test is in repo '{repo}', this tool can only disable tests in " +
        "chromium/chromium/src")

  return name, filename


def get_test_result_history(test_id: str, page_size: int) -> dict:
  """Make a GetTestResultHistory RPC call to ResultDB.

  Args:
    test_id: The full test ID to query. This can be a regex.
    page_size: The number of results to return within the first response.

  Returns:
    The GetTestResultHistoryResponse message, in dict form.
  """

  now = datetime.datetime.now(datetime.timezone.utc)
  request = {
      'realm': 'chromium:ci',
      'testIdRegexp': test_id,
      'timeRange': {
          'earliest': (now - datetime.timedelta(hours=6)).isoformat(),
          'latest': now.isoformat(),
      },
      'pageSize': page_size,
  }

  return rdb_rpc('GetTestResultHistory', request)


def get_test_result(test_name: str) -> dict:
  """Make a GetTestResult RPC call to ResultDB.

  Args:
    test_name: The name of the test result to query. This specifies a result for
      a particular test, within a particular test run. As returned by
      GetTestResultHistory.

  Returns:
    The TestResult message, in dict form.
  """

  return rdb_rpc('GetTestResult', {
      'name': test_name,
  })


def query_test_result(invocation: str, test_regex: str):
  """Make a QueryTestResults RPC call to ResultDB.

  Args:
    invocation_name: the name of the invocation to query for test results
    test_regex: The test regex to filter

  Returns:
    The QueryTestResults response message, in dict form.
  """
  request = {
      'invocations': [invocation],
      'readMask': {
          'paths': ['test_id', 'test_metadata'],
      },
      'pageSize': 1000,
      'predicate': {
          'testIdRegexp': test_regex,
      },
  }
  return rdb_rpc('QueryTestResults', request)


# Used for caching RPC responses, for development purposes.
CANNED_RESPONSE_FILE: Optional[str] = None


def rdb_rpc(method: str, request: dict) -> dict:
  """Call the given RPC method, with the given request.

  Args:
    method: The method to call. Must be within luci.resultdb.v1.ResultDB.
    request: The request, in dict format.

  Returns:
    The response from ResultDB, in dict format.
  """

  if CANNED_RESPONSE_FILE is not None:
    try:
      with open(CANNED_RESPONSE_FILE, 'r') as f:
        canned_responses = json.load(f)
    except Exception:
      canned_responses = {}

    # HACK: Strip out timestamps when caching the request. GetTestResultHistory
    # includes timestamps based on the current time, which will bust the cache.
    # But for e2e testing we just want to cache the result the first time and
    # then keep using it.
    if 'timeRange' in request:
      key_request = dict(request)
      del key_request['timeRange']
    else:
      key_request = request

    key = f'{method}/{json.dumps(key_request)}'
    if (response_json := canned_responses.get(key, None)) is not None:
      return json.loads(response_json)

  p = subprocess.Popen(['rdb', 'rpc', 'luci.resultdb.v1.ResultDB', method],
                       stdin=subprocess.PIPE,
                       stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE,
                       text=True)

  stdout, stderr = p.communicate(json.dumps(request))
  if p.returncode != 0:
    # rdb doesn't return unique status codes for different errors, so we have to
    # just match on the output.
    if 'interactive login is required' in stderr:
      raise errors.UserError(
          "Authentication is required to fetch test metadata.\n" +
          "Please run:\n\trdb auth-login\nand try again")

    raise Exception(f'rdb rpc {method} failed with: {stderr}')

  if CANNED_RESPONSE_FILE:
    canned_responses[key] = stdout
    with open(CANNED_RESPONSE_FILE, 'w') as f:
      json.dump(canned_responses, f)

  return json.loads(stdout)
