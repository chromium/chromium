# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Checks for new additions of Polymer (rather than Lit) imports.
"""

from . import regex_check


def AddedPolymerImportsCheck(input_api, output_api, file_filter=lambda f: True):
  results = []

  def PolymerImportCheck(line_number, line):
    return regex_check.RegexCheck(
        input_api.re, line_number, line,
        r"(\/\/resources\/polymer\/v3_0\/polymer\/polymer_bundled\.min\.js)",
        "Polymer is deprecated, new elements should be added in Lit instead")

  def AddedPolymerImportsFilter(affected_file):
    # Fastest way to see if a file is added and has a .ts extension. Imports
    # won't appear in HTML or other files.
    return affected_file.Action() == 'A' and \
        affected_file.LocalPath().endswith('.ts')

  # Call AddedPolymerImportsFilter first because it is a cheap filter that will
  # reject most files, thus reducing the number of calls to file_filter which
  # might be expensive.
  wrapped_filter = lambda f: AddedPolymerImportsFilter(f) and file_filter(f)
  added_files = input_api.AffectedFiles(include_deletes=False,
                                        file_filter=wrapped_filter)

  for f in added_files:
    error_lines = []

    for i, line in f.ChangedContents():
      error_lines += [_f for _f in [
          PolymerImportCheck(i, line),
      ] if _f]

    if error_lines:
      error_lines = [
          "Found JavaScript style violations in %s:" % f.LocalPath()
      ] + error_lines
      results.append(output_api.PresubmitError("\n".join(error_lines)))

  return results
