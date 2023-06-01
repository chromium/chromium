# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs python unittests.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""



def _CommonChecks(input_api, output_api):
  results = []
  results.extend(
      input_api.canned_checks.RunPylint(input_api, output_api, version='2.6'))

  commands = []
  commands.extend(
      input_api.canned_checks.GetUnitTestsRecursively(
          input_api,
          output_api,
          input_api.os_path.join(input_api.PresubmitLocalPath()),
          files_to_check=[r'.+_unittest\.py$'],
          files_to_skip=[]))
  results.extend(input_api.RunTests(commands))

  return results


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
