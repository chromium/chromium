# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for changes affecting partition_alloc.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'


# Make sure no `.actual` file is added in tests:
def CheckAllowedFiles(input_api, output_api):
  errors = []
  for f in input_api.AffectedFiles(include_deletes=False):
    path = f.LocalPath()
    if not path.endswith(('.py', '.cpp', '.txt', '.h')):
      errors.append(output_api.PresubmitError("Unexpected file added %s" %
                                              path))
  return errors
