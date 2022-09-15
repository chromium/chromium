# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import six.moves.urllib.parse  # pylint: disable=import-error
import six.moves.urllib.request  # pylint: disable=import-error


TEST_RESULTS_SERVER = 'http://test-results.appspot.com'
TOOL_USER_AGENT = 'flakiness_cli/0.1'


def _Request(path, params=None):
  """Request json data from the test results server."""
  url = TEST_RESULTS_SERVER + path
  if params:
    url += '?' + six.moves.urllib.parse.urlencode(params)
  request = six.moves.urllib.request.Request(
      url, headers={'User-Agent': TOOL_USER_AGENT})
  response = six.moves.urllib.request.urlopen(request)
  return json.load(response)


def GetBuilders():
  """Retrieve json with list of all known masters, builders, and test_types."""
  return _Request('/data/builders')


def GetTestResults(master, builder, test_type):
  """Retrieve test results json for a given master, builder, and test_type."""
  return _Request('/testfile', {
      'name': 'results.json',
      'master': master,
      'builder': builder,
      'testtype': test_type})
