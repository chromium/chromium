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
# TODO(crbug.com/359623676): Fix the lint errors in these files and remove them
# from the list.
PYLINT_FILES_TO_SKIP = [
    'code_util.py',
    'compiler.py',
    'cpp_generator.py',
    'cpp_namespace_environment.py',
    'cpp_bundle_generator.py',
    'cpp_type_generator.py',
    'cpp_type_generator_test.py',
    'cc_generator.py',
    'features_cc_generator.py',
    'features_compiler.py',
    'features_h_generator.py',
    'highlighters/hilite_me_highlighter.py',
    'highlighters/none_highlighter.py',
    'highlighters/pygments_highlighter.py',
    'generate_all_externs.py',
    'feature_compiler.py',
    'h_generator.py',
    'js_externs_generator.py',
    'idl_schema.py',
    'json_schema.py',
    'js_interface_generator.py',
    'js_util.py',
    'namespace_resolver.py',
    'schema_loader.py',
    'model.py',
    'util_cc_helper.py',
    'idl_schema_test.py',
    'ts_definition_generator.py',
    'preview.py',
]


def CheckExterns(input_api, output_api):
  """Make sure tool changes update the generated externs."""
  original_sys_path = sys.path
  try:
    sys.path.insert(0, input_api.PresubmitLocalPath())
    # pylint: disable=import-outside-toplevel
    from generate_all_externs import Generate
    # pylint: enable=import-outside-toplevel
  finally:
    sys.path = original_sys_path

  return Generate(input_api, output_api, dryrun=True)


def CheckPylint(input_api, output_api):
  return input_api.canned_checks.RunPylint(
      input_api, output_api, version='2.7', files_to_skip=PYLINT_FILES_TO_SKIP
  )


def CheckTests(input_api, output_api):
  return input_api.canned_checks.RunUnitTestsInDirectory(
      input_api, output_api, '.', files_to_check=TEST_FILE_PATTERN
  )
