# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys

_FILE_PATH = os.path.dirname(os.path.realpath(__file__))
_SYS_PATH = sys.path[:]
try:
  _COMMENT_EATER_PATH = os.path.join(_FILE_PATH, os.pardir,
                                     'json_comment_eater')
  sys.path.insert(0, _COMMENT_EATER_PATH)
  import json_comment_eater
finally:
  sys.path = _SYS_PATH

try:
  from collections import OrderedDict

  # Successfully imported, so we're running Python >= 2.7, and json.loads
  # supports object_pairs_hook.
  def Parse(json_str):
    return json.loads(json_comment_eater.Nom(json_str),
                      object_pairs_hook=OrderedDict)

except ImportError:
  # Failed to import, so we're running Python < 2.7, and json.loads doesn't
  # support object_pairs_hook. simplejson however does, but it's slow.
  #
  # TODO(cduvall/kalman): Refuse to start the docs server in this case, but
  # let json-schema-compiler do its thing.
  #logging.warning('Using simplejson to parse, this might be slow! Upgrade to '
  #                'Python 2.7.')

  _SYS_PATH = sys.path[:]
  try:
    _SIMPLE_JSON_PATH = os.path.join(_FILE_PATH, os.pardir, os.pardir,
                                     'third_party')
    sys.path.insert(0, _SIMPLE_JSON_PATH)
    # Add this path in case this is being used in the docs server.
    sys.path.insert(
        0,
        os.path.join(_FILE_PATH, os.pardir, os.pardir, 'third_party',
                     'json_schema_compiler'))
    import simplejson
    from simplejson import OrderedDict
  finally:
    sys.path = _SYS_PATH

  def Parse(json_str):
    return simplejson.loads(json_comment_eater.Nom(json_str),
                            object_pairs_hook=OrderedDict)


def IsDict(item):
  return isinstance(item, (dict, OrderedDict))
