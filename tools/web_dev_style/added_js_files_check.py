# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Checks for new additions of JS (rather than TS) files.
"""


def AddedJsFilesCheck(input_api, output_api, file_filter=lambda f: True):
  results = []

  def AddedJsFilesFilter(affected_file):
    # Fastest way to see if a file has a .js extension.
    return affected_file.LocalPath().endswith('.js')

  # Call AddedJsFilesFilter first because it is a cheap filter that will reject
  # most files, thus reducing the number of calls to file_filter which might be
  # expensive.
  wrapped_filter = lambda f: AddedJsFilesFilter(f) and file_filter(f)
  added_js_files = input_api.AffectedFiles(include_deletes=False,
                                           file_filter=wrapped_filter)

  for f in added_js_files:
    results += [
        output_api.PresubmitError(
            'Disallowed JS file found \'%s\'. New WebUI files must be written '
            'in TypeScript instead.' % f.LocalPath())
    ]

  return results
