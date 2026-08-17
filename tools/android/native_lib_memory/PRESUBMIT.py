# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for native_lib_memory.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""


def CommonChecks(input_api, output_api):
  output = []
  files_to_skip = []
  disabled_warnings = [
    'anomalous-backslash-in-string',
    'bad-indentation',
    'consider-using-enumerate',
    'consider-using-with',
    'duplicate-code',
    'logging-not-lazy',
    'missing-module-docstring',
    'protected-access',
    'unnecessary-semicolon',
    'unspecified-encoding',
    'unused-import',
  ]
  output.extend(
    input_api.canned_checks.RunPylint(
      input_api,
      output_api,
      disabled_warnings=disabled_warnings,
      files_to_skip=files_to_skip,
      version='3.2',
    )
  )
  # These tests don't run on Windows and give verbose and cryptic failure
  # messages.
  if input_api.sys.platform != 'win32':
    output.extend(
      input_api.canned_checks.RunUnitTests(
        input_api,
        output_api,
        [input_api.os_path.join(input_api.PresubmitLocalPath(), 'run_tests')],
      )
    )
  return output


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)
