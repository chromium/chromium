# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy

import json_parse


def Load(filename):
  try:
    with open(filename, 'rb') as handle:
      schemas = json_parse.Parse(handle.read().decode('utf8'))
    return schemas
  except:
    print('FAILED: Exception encountered while loading "%s"' % filename)
    raise


# A dictionary mapping |filename| to the object resulting from loading the JSON
# at |filename|.
_cache = {}


def CachedLoad(filename):
  """Equivalent to Load(filename), but caches results for subsequent calls"""
  if filename not in _cache:
    _cache[filename] = Load(filename)
  # Return a copy of the object so that any changes a caller makes won't affect
  # the next caller.
  return copy.deepcopy(_cache[filename])
