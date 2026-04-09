# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def _CheckNoSchemaModifications(input_api, output_api):
  """Ensures that schema files are only added or deleted, never modified."""

  def IsIdlFile(file):
    # We don't currently have any .json schemas in this folder, but plan to also
    # start converting JSON schemas to WebIDL in the near future.
    return file.LocalPath().endswith((".idl", ".json", ".webidl"))

  modified_files = []

  # Actions can be 'A' (Added), 'D' (Deleted), or 'M' (Modified). We only care
  # about modification for sending a warning here.
  for file in input_api.AffectedFiles(include_deletes=False,
                                      file_filter=IsIdlFile):
    if file.Action() == "M":
      modified_files.append(file.LocalPath())

  if not modified_files:
    return []

  message = (
      "You are modifying existing API schema files in this directory.\n"
      "Files in this directory are meant to serve as a static baseline for the "
      "IDL to WebIDL conversion process. They should only be added at the time "
      "of conversion and do not need to be updated afterwards.\n\n"
      "Please refer to the README in this directory for more information.\n\n"
      "Modified files:\n  " + "\n  ".join(modified_files))

  return [output_api.PresubmitPromptWarning(message)]


def CheckChangeOnUpload(input_api, output_api):
  return _CheckNoSchemaModifications(input_api, output_api)
