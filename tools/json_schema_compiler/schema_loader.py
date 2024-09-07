# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

import idl_schema
import json_schema


class SchemaLoader(object):
  '''Loads a schema from a provided filename.
  |root|: path to the root directory.
  '''

  def __init__(self, root):
    self._root = root

  def LoadSchema(self, schema):
    '''Load a schema definition. The schema parameter must be a file name
    with the full path relative to the root.'''
    _, schema_extension = os.path.splitext(schema)

    schema_path = os.path.join(self._root, schema)
    if schema_extension == '.json':
      api_defs = json_schema.Load(schema_path)
    elif schema_extension == '.idl':
      api_defs = idl_schema.Load(schema_path)
    else:
      sys.exit('Did not recognize file extension %s for schema %s' %
               (schema_extension, schema))

    # TODO(devlin): This returns a list. Does it need to? Is it ever > 1?
    return api_defs
