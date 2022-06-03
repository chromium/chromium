# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for liblouis repository."""

import json
import sys

USE_PYTHON3 = True

def CheckChangeOnUpload(input_api, output_api):
  if not sys.platform.startswith('linux'):
    return []
  sys.path.insert(0, input_api.os_path.join(
      input_api.PresubmitLocalPath()))
  try:
    import liblouis_list_tables
  finally:
    sys.path.pop(0)
  errors, new_tables = liblouis_list_tables.CheckTables("tables.json")
  results = []
  for x in errors:
    results.append(output_api.PresubmitError(x))

  # Write a suggested json for new tables.
  if len(new_tables) > 0:
    new_json = []
    for table in new_tables:
      name = table.split('.')[0]
      name_parts = name.split('-')

      # These are guesses as to the grade and dots based on the filename. The
      # suggestion still needs to be validated.
      dots = "6"
      if "comp8" in name:
        dots = "8"

      grade = "1"
      if "g0" in name:
        grade = "0"
      elif "g2" in name:
        grade = "2"
      elif "g3" in name:
        grade = "3"

      new_json.append({
          "id": name,
          "locale": name_parts[0],
          "dots": dots,
          "grade": grade,
          "fileNames": table
      })
    results.append(output_api.PresubmitError("Suggested additions to " +
                                             "tables.json (please edit and validate):"))
    results.append(output_api.PresubmitError(json.dumps(new_json, indent=2)))
  return results
