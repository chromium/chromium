# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Checks for new additions of JS (rather than TS) files.
"""


def AddedJsFilesCheck(input_api, output_api, file_filter=lambda f: True):
  results = []

  def AddedJsFilesFilter(affected_file):
    filename = input_api.os_path.basename(affected_file.LocalPath())
    _, extension = input_api.os_path.splitext(filename)
    return extension == '.js'

  wrapped_filter = lambda f: file_filter(f) and AddedJsFilesFilter(f)
  added_js_files = input_api.AffectedFiles(include_deletes=False,
                                           file_filter=wrapped_filter)

  for f in added_js_files:
    results += [
        output_api.PresubmitError(
            'Disallowed JS file found \'%s\'. New WebUI files must be written '
            'in TypeScript instead.' % f.LocalPath())
    ]

  return results
