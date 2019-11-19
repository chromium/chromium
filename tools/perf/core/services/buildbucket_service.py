# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Make requests to the Buildbucket RPC API.

For more details on the API see: go/buildbucket-rpc
"""

from core.services import request


SERVICE_URL = 'https://cr-buildbucket.appspot.com/prpc/buildbucket.v2.Builds/'


def Request(method, **kwargs):
  """Send a request to some buildbucket service method."""
  kwargs.setdefault('use_auth', True)
  kwargs.setdefault('method', 'POST')
  kwargs.setdefault('content_type', 'json')
  kwargs.setdefault('accept', 'json')
  return request.Request(SERVICE_URL + method, **kwargs)


def GetBuild(project, bucket, builder, build_number):
  """Get the status of a build by its build number.

  Args:
    project: The LUCI project name (e.g. 'chromium').
    bucket: The LUCI bucket name (e.g. 'ci' or 'try').
    builder: The builder name (e.g. 'linux_chromium_rel_ng').
    build_number: An int with the build number to get.
  """
  return Request('GetBuild', data={
      'builder': {
          'project': project,
          'bucket': bucket,
          'builder': builder,
      },
      'buildNumber': build_number,
  })


def GetBuilds(project, bucket, builder, only_completed=True):
  """Get a list of recent builds from a given builder.

  Args:
    project: The LUCI project name (e.g. 'chromium').
    bucket: The LUCI bucket name (e.g. 'ci' or 'try').
    builder: The builder name (e.g. 'linux_chromium_rel_ng').
    only_completed: An optional bool to indicate whehter builds that have
      not yet finished should be included in the results. The default is to
      include only completed builds.
  """
  data = {
      'predicate': {
          'builder': {
              'project': project,
              'bucket': bucket,
              'builder': builder
          }
      }
  }
  if only_completed:
    data['predicate']['status'] = 'ENDED_MASK'
  return Request('SearchBuilds', data=data)
