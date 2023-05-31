# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for liblouis repository."""

import json
import sys


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

      # See if the table file has any more data.
      data = liblouis_list_tables.GetAdditionalFileTableData("src/tables/" + table)
      if "dots" in data:
        dots = data["dots"]

      en_display_name = ""
      if "display-name" in data:
        en_display_name = data["display-name"]

      locale = name_parts[0]
      if 'locale' in data:
        locale = data['locale']
      elif len(locale) == 4:
        # This works around bad locale specifiers e.g. "zhcn".
        locale = locale[0:2] + "-" + locale[2:]
      elif len(name_parts) > 1 and len(name_parts[1]) == 2:
        locale = "-".join(name_parts[0:2])

      entry = {
        "id": name,
        "locale": locale,
        "dots": dots,
        "grade": grade,
        "fileNames": table
      }

      if en_display_name:
        entry["enDisplayName"] = en_display_name

      new_json.append(entry)

    results.append(output_api.PresubmitNotifyResult("Suggested additions to " +
                                             "tables.json (please edit and validate):\n" +
                                             json.dumps(new_json, indent=2)))
  return results
