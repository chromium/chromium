# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core.services import luci_auth
from core.services import request


SERVICE_URL = 'https://pinpoint-dot-chromeperf.appspot.com/api'


def Request(endpoint, **kwargs):
  """Send a request to some pinpoint endpoint."""
  kwargs.setdefault('use_auth', True)
  kwargs.setdefault('accept', 'json')
  return request.Request(SERVICE_URL + endpoint, **kwargs)


def Job(job_id, with_state=False, with_tags=False):
  """Get job information from its id."""
  params = []
  if with_state:
    params.append(('o', 'STATE'))
  if with_tags:
    params.append(('o', 'TAGS'))
  return Request('/job/%s' % job_id, params=params)


def Jobs():
  """List jobs for the authenticated user."""
  return Request('/jobs')


def NewJob(**kwargs):
  """Create a new pinpoint job."""
  if 'user' not in kwargs:
    kwargs['user'] = luci_auth.GetUserEmail()
  return Request('/new', method='POST', data=kwargs)
