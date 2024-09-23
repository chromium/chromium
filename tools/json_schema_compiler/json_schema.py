# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy

import json_parse


def DeleteNodes(item, delete_key=None, matcher=None):
  """Deletes certain nodes in item, recursively. If |delete_key| is set, all
  dicts with |delete_key| as an attribute are deleted. If a callback is passed
  as |matcher|, |DeleteNodes| will delete all dicts for which matcher(dict)
  returns True.
  """
  assert (delete_key is not None) != (matcher is not None)

  def ShouldDelete(thing):
    return json_parse.IsDict(thing) and (
        delete_key is not None and delete_key in thing
        or matcher is not None and matcher(thing))

  if json_parse.IsDict(item):
    toDelete = []
    for key, value in item.items():
      if ShouldDelete(value):
        toDelete.append(key)
      else:
        DeleteNodes(value, delete_key, matcher)
    for key in toDelete:
      del item[key]
  elif type(item) == list:
    item[:] = [
        DeleteNodes(thing, delete_key, matcher) for thing in item
        if not ShouldDelete(thing)
    ]

  return item


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
