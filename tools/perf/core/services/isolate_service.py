# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import json
import os
import zlib

from core.services import request


SERVICE_URL = 'https://chrome-isolated.appspot.com/_ah/api/isolateservice/v1'
CACHE_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '_cached_data', 'isolates'))


def Request(endpoint, **kwargs):
  """Send a request to some isolate service endpoint."""
  kwargs.setdefault('use_auth', True)
  kwargs.setdefault('accept', 'json')
  return request.Request(SERVICE_URL + endpoint, **kwargs)


def Retrieve(digest):
  """Retrieve the content stored at some isolate digest."""
  return zlib.decompress(RetrieveCompressed(digest))


def RetrieveFile(digest, filename):
  """Retrieve a particular filename from an isolate container."""
  container = json.loads(Retrieve(digest))
  return Retrieve(container['files'][filename]['h'])


def RetrieveCompressed(digest):
  """Retrieve the compressed content stored at some isolate digest.

  Responses are cached locally to speed up retrieving content multiple times
  for the same digest.
  """
  cache_file = os.path.join(CACHE_DIR, digest)
  if os.path.exists(cache_file):
    with open(cache_file, 'rb') as f:
      return f.read()
  else:
    if not os.path.isdir(CACHE_DIR):
      os.makedirs(CACHE_DIR)
    content = _RetrieveCompressed(digest)
    with open(cache_file, 'wb') as f:
      f.write(content)
    return content


def _RetrieveCompressed(digest):
  """Retrieve the compressed content stored at some isolate digest."""
  data = Request(
      '/retrieve', method='POST', content_type='json',
      data={'namespace': {'namespace': 'default-gzip'}, 'digest': digest})

  if 'url' in data:
    return request.Request(data['url'])
  if 'content' in data:
    return base64.b64decode(data['content'])
  else:
    raise NotImplementedError(
        'Isolate %s in unknown format %s' % (digest, json.dumps(data)))
