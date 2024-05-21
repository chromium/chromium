# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for changes affecting tools/json_schema_compiler/

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import sys

FILE_PATTERN = [ r'.+_test.py$' ]


def _CheckExterns(input_api, output_api):
  """Make sure tool changes update the generated externs."""
  original_sys_path = sys.path
  try:
    sys.path.insert(0, input_api.PresubmitLocalPath())
    from generate_all_externs import Generate
  finally:
    sys.path = original_sys_path

  return Generate(input_api, output_api, dryrun=True)


def _GetFilesToSkip(input_api):
  """Returns the list of test files to skip.

  The clang-format hook is being migrated for cog and until that is complete,
  tests that rely on clang-format are unsupported on cog.

  TODO(b/333744051): Remove this method when clang-format is fully migrated.
  """
  files_to_skip = []
  if input_api.change.scm != 'git':
      files_to_skip.append('.*ts_definition_generator_test\.py')
  return files_to_skip


def CheckChangeOnUpload(input_api, output_api):
  ret = input_api.canned_checks.RunUnitTestsInDirectory(
      input_api,
      output_api,
      '.',
      files_to_check=FILE_PATTERN,
      files_to_skip=_GetFilesToSkip(input_api))
  ret += _CheckExterns(input_api, output_api)
  return ret


def CheckChangeOnCommit(input_api, output_api):
  ret = input_api.canned_checks.RunUnitTestsInDirectory(
      input_api, output_api, '.', files_to_check=FILE_PATTERN,
      files_to_skip=_GetFilesToSkip(input_api))
  ret += _CheckExterns(input_api, output_api)
  return ret
