# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for linux.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""


import sys


def CommonChecks(input_api, output_api):
  def join(*args):
    return input_api.os_path.join(input_api.PresubmitLocalPath(), *args)

  output = []
  sys_path_backup = sys.path
  try:
    sys.path = [
      join('..', 'linux'),
    ] + sys.path
    output.extend(
        input_api.canned_checks.RunPylint(input_api, output_api, version='2.7'))
  finally:
    sys.path = sys_path_backup

  output.extend(
      input_api.canned_checks.RunUnitTestsInDirectory(
          input_api,
          output_api,
          input_api.os_path.join(input_api.PresubmitLocalPath(), 'tests'),
          files_to_check=[r'.+_tests\.py$']))
  return output


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)
