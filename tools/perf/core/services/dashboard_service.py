# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Make requests to the Chrome Perf Dashboard API.

For more details on the API see:
https://chromium.googlesource.com/catapult.git/+/HEAD/dashboard/dashboard/api/README.md
"""

import six.moves.urllib.parse  # pylint: disable=import-error

from core.services import request

SERVICE_URL = 'https://chromeperf.appspot.com'


def Request(endpoint, **kwargs):
  """Send a request to some dashboard service endpoint."""
  kwargs.setdefault('use_auth', True)
  kwargs.setdefault('method', 'POST')
  kwargs.setdefault('accept', 'json')
  return request.Request(SERVICE_URL + endpoint, **kwargs)


def Describe(test_suite):
  """Obtain information about a given test_suite.

  Args:
    test_suite: A string with the name of the test suite.

  Returns:
    A dict with information about: bots, caseTags, cases, and measurements.
  """
  return Request('/api/describe', params={'test_suite': test_suite})


def Timeseries2(**kwargs):
  """Get timeseries data for a particular test path.

  Args:
    test_suite: A string with the test suite or benchmark name.
    measurement: A string with the metric name, e.g. timeToFirstContentfulPaint.
    bot: A string with the bot name, usually of the form 'master:builder'.
    columns: A string with a comma separated list of column names to retrieve;
      may contain: revision, avg, std, count, max, min, sum, revisions,
      timestamp, alert, histogram, diagnostics.
    test_case: An optional string with the name of a test case or story.
    **kwargs: For other options and full details see the API docs.

  Returns:
    A dict with timeseries data, alerts, Histograms, and SparseDiagnostics.

  Raises:
    TypeError if any required arguments are missing.
    KeyError if the timeseries is not found.
  """
  for col in ('test_suite', 'measurement', 'bot', 'columns'):
    if col not in kwargs:
      raise TypeError('Missing required argument: %s' % col)
  try:
    return Request('/api/timeseries2', params=kwargs)
  except request.ClientError as exc:
    if exc.response.status == 404:
      raise KeyError('Timeseries not found')
    raise  # Re-raise the original exception.


def Timeseries(test_path, days=30):
  """Get timeseries for the given test path.

  TODO(crbug.com/40603244): Remove when no longer needed.

  Args:
    test_path: test path to get timeseries for.
    days: Number of days to get data points for.

  Returns:
    A dict with timeseries data for the given test_path

  Raises:
    KeyError if the test_path is not found.
  """
  try:
    return Request('/api/timeseries/%s' %
                   six.moves.urllib.parse.quote(test_path),
                   params={'num_days': days})
  except request.ClientError as exc:
    if 'Invalid test_path' in exc.json['error']:
      raise KeyError(test_path)
    raise


def ListTestPaths(test_suite, sheriff):
  """Lists test paths for the given test_suite.

  TODO(crbug.com/40603244): Remove when no longer needed.

  Args:
    test_suite: String with test suite to get paths for.
    sheriff: Include only test paths monitored by the given sheriff rotation,
        use 'all' to return all test paths regardless of rotation.

  Returns:
    A list of test paths. Ex. ['TestPath1', 'TestPath2']
  """
  return Request(
      '/api/list_timeseries/%s' % test_suite, params={'sheriff': sheriff})


def MatchTestPaths(pattern, only_with_rows=False):
  """Lists test paths matching a given pattern.

  The exact semantics of these patterns is defined by the implementation of
  the dashboard.common.utils.TestMatchesPattern function.

  Args:
    pattern: String with glob-like pattern notation to match.
    only_with_rows: Only match test paths which have data points.

  Returns:
    A list of test paths. Ex. ['TestPath1', 'TestPath2']
  """
  params = {'type': 'pattern', 'p': pattern}
  if only_with_rows:
    params['has_rows'] = '1'
  return Request('/list_tests', params=params)


def Bugs(bug_id):
  """Get all the information about a given bug id."""
  return Request('/api/bugs/%d' % bug_id)


def IterAlerts(**kwargs):
  """Returns alerts matching the supplied query parameters.

  The response for the dashboard may be returned in multiple chunks, this
  function will take care of following `next_cursor`s in responses and
  iterate over all the chunks.

  Args:
    test_suite: Match alerts on a given test suite (benchmark).
    sheriff: Match only alerts of a given sheriff rotation.
    min_timestamp, max_timestamp: Match only alerts on a given time range.
    limit: Max number of responses per chunk (defaults to 1000).
    **kwargs: See API docs for other possible query params.

  Yields:
    Data for all the matching alerts in chunks.
  """
  kwargs.setdefault('limit', 1000)
  while True:
    response = Request('/api/alerts', params=kwargs)
    yield response
    if 'next_cursor' in response:
      kwargs['cursor'] = response['next_cursor']
    else:
      return
