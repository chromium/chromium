# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""binary_size presubmit script

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'


def CheckPyLint(input_api, output_api):
  output = []
  # These tools don't run on Windows so these tests don't work and give many
  # verbose and cryptic failure messages.
  if input_api.sys.platform != 'win32':
    output.extend(
        input_api.canned_checks.RunPylint(input_api, output_api, version='2.6'))
  return output


def CheckRunUnitTests(input_api, output_api):
  output = []
  # Linting the code is skipped on Windows because it will fail due to OS
  # differences.
  if input_api.sys.platform != 'win32':
    py_tests = input_api.canned_checks.GetUnitTestsRecursively(
        input_api,
        output_api,
        input_api.PresubmitLocalPath(),
        files_to_check=[r'.+_test\.py$'],
        files_to_skip=[])
    output.extend(input_api.RunTests(py_tests, False))
  return output


def CheckPathFormatted(input_api, output_api):
  return input_api.canned_checks.CheckPatchFormatted(input_api,
                                                     output_api,
                                                     check_js=True)
