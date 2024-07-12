# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Make requests to the ResultDB RPC API.

For more details on the API see: go/resultdb-rpc
"""

from core.services import request

SERVICE_URL = 'https://results.api.cr.dev/prpc/luci.resultdb.v1.ResultDB/'


def Request(method, **kwargs):
  """Send a request to some resultdb service method."""
  kwargs.setdefault('use_auth', True)
  kwargs.setdefault('method', 'POST')
  kwargs.setdefault('content_type', 'json')
  kwargs.setdefault('accept', 'json')
  return request.Request(SERVICE_URL + method, **kwargs)


def GetQueryTestResult(buildbucket_id):
  """Get the test results of a build by its buildbucket id.

  Args:
    buildbucket_id: An int with the build number to get.
  """
  invocation_id = f'invocations/build-{buildbucket_id}'
  return Request('QueryTestResults',
                 data={
                     'invocations': [invocation_id],
                     'predicate': {
                         'expectancy': 'VARIANTS_WITH_UNEXPECTED_RESULTS'
                     }
                 })


def GetQueryTestResults(buildbucket_ids):
  """Get a list of test results from a list of builds.

  Args:
    buildbucket_ids: A list of buildbucket ids.
  """
  invocation_ids = list(map(lambda x: f'invocations/build-{x}',
                            buildbucket_ids))
  data = {
      'invocations': invocation_ids,
      'predicate': {
          'expectancy': 'VARIANTS_WITH_UNEXPECTED_RESULTS'
      }
  }

  return Request('QueryTestResults', data=data)
