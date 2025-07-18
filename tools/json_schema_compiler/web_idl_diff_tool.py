#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys

from difflib import unified_diff
from schema_loader import SchemaLoader

# Utility for running two separate API schema files through the SchemaLoader and
# comparing the output of each. Intended to be used when converting an extension
# API schema file from one format to another i.e. from the old IDL or JSON
# format to WebIDL.
#
# This file can be run manually, but is also used in automated tests for schema
# files which have been converted to WebIDL to ensure there is no functional
# changes introduced during the conversion process.


def LoadAndReturnUnifiedDiff(file_one: str, file_two: str) -> str:
  """Loads two schemas passed in and returns any differences from the output.

  Args:
    file_one: Filepath string to the first schema.
    file_two: Filepath string to the second schema.

  Returns:
    A human readable string containing the diff between the outputs of the
    SchemaLoader in the unified_diff format. May be an empty string if there is
    no difference detected.
  """
  root = os.path.join(os.path.dirname(os.path.realpath(__file__)), os.pardir,
                      os.pardir)
  schema_one = SchemaLoader(root).LoadSchema(file_one)
  schema_two = SchemaLoader(root).LoadSchema(file_two)

  difference = unified_diff(
      json.dumps(schema_one, indent=2, sort_keys=True).splitlines(),
      json.dumps(schema_two, indent=2, sort_keys=True).splitlines())

  diff_list = list(difference)
  if not diff_list:
    return ''
  return '\n'.join(diff_list)


if __name__ == "__main__":
  args = sys.argv[1:]
  if len(args) != 2:
    raise Exception(
        'Must be called with two parameters, each a file path to an API schema'
        ' file you want to compare the differences between after they are both'
        ' parsed and processed.')

  diff = LoadAndReturnUnifiedDiff(args[0], args[1])
  if diff:
    print(diff)
  else:
    print('No difference found!')
