#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import web_idl_diff_tool

# Test to ensure there are no difference in parsing + processing extension API
# schemas when converting them to be written using WebIDL. Uses the alarms API
# schema files as an example to test, copied over at the time they were
# initially converted to WebIDL.


class WebIdlDiffToolTest(unittest.TestCase):

  def testIdlToWebIdlConversion(self):
    converted_schemas = [
        ('alarms.idl', 'alarms.webidl'),
    ]
    # LoadAndReturnUnifiedDiff expects file paths relative to the repo root.
    converted_schema_path = 'tools/json_schema_compiler/test/converted_schemas/'
    for old_schema_name, new_schema_name in converted_schemas:
      old_filename = converted_schema_path + old_schema_name
      new_filename = converted_schema_path + new_schema_name
      diff = web_idl_diff_tool.LoadAndReturnUnifiedDiff(old_filename,
                                                        new_filename)
      self.assertEqual(
          '',
          diff,
          f"Difference detected between '{old_filename}' and"
          f" '{new_filename}':\n{diff}",
      )


if __name__ == '__main__':
  unittest.main()
