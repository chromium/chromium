# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for cygprofile.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""


def CommonChecks(input_api, output_api):
  output = []
  files_to_skip = []
  output.extend(
      input_api.canned_checks.RunPylint(input_api,
                                        output_api,
                                        files_to_skip=files_to_skip,
                                        version='2.7'))

  # These tests don't run on Windows and give verbose and cryptic failure
  # messages.
  if input_api.sys.platform != 'win32':
    output.extend(
        input_api.canned_checks.RunUnitTests(input_api, output_api, [
            input_api.os_path.join(input_api.PresubmitLocalPath(), 'run_tests')
        ]))
  return output


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)
