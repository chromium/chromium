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


def get_test_metadata(test_id: str) -> Tuple[str, str]:
  """Fetch test metadata from ResultDB.

  Args:
    test_id: The full ID of the test to fetch. For Chromium tests this will
    begin with "ninja://"
  Returns:
    A tuple of (test name, filename). The test name will for example have the
    form  SuitName.TestName for GTest tests. The filename is the location in the
    source tree where this test is defined.
  """

  history = get_test_result_history(test_id, 1)

  if 'entries' not in history:
    raise errors.UserError(
        f"ResultDB query couldn't find test with ID: {test_id}")

  # TODO: Are there other statuses we need to exclude?
  if history['entries'][0]['result'].get('status', '') == 'SKIP':
    # Test metadata isn't populated for skipped tests. Ideally this wouldn't be
    # the case, but for now we just work around it.
    # On the off-chance this test is conditionally disabled, we request a bit
    # more history in the hopes of finding a test run where it isn't skipped.
    history = get_test_result_history(test_id, 10)

    for entry in history['entries'][1:]:
      if entry['result'].get('status', '') != 'SKIP':
        break
    else:
      raise errors.UserError(
          f"Unable to fetch metadata for test {test_id}, as the last 10 runs " +
          "in ResultDB have status 'SKIP'. Is the test already disabled?")
  else:
    entry = history['entries'][0]

  try:
    inv_name = entry['result']['name']
  except KeyError as e:
    raise errors.InternalError(
        f"Malformed GetTestResultHistory response: no key {e}") from e
  # Ideally GetTestResultHistory would return metadata so we could avoid making
  # this second RPC.
  result = get_test_result(inv_name)

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
