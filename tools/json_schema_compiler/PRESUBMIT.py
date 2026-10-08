# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for changes affecting tools/json_schema_compiler/

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import sys

PRESUBMIT_VERSION = '2.0.0'
TEST_FILE_PATTERN = [r'.+_test.py$']


def CheckExterns(input_api, output_api):
  """Make sure tool changes update the generated externs."""
  if not input_api.HasAffectedFiles(extensions=('.py', '.json', '.idl')):
    return []
  original_sys_path = sys.path
  try:
    sys.path.insert(0, input_api.PresubmitLocalPath())
    from generate_all_externs import Generate
  finally:
    sys.path = original_sys_path

  return Generate(input_api, output_api, dryrun=True)


def CheckTests(input_api, output_api):
  if not (
    input_api.HasAffectedFiles(extensions='.py')
    or input_api.HasAffectedFiles(path='test')
  ):
    return []
  return input_api.canned_checks.RunUnitTestsInDirectory(
    input_api, output_api, '.', files_to_check=TEST_FILE_PATTERN
  )
