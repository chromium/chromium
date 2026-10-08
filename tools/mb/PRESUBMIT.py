# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


PRESUBMIT_VERSION = '2.0.0'


def CheckFreeze(input_api, output_api):
  return input_api.canned_checks.CheckInfraFreeze(input_api, output_api)


def CheckTests(input_api, output_api):
  if not input_api.HasAffectedFiles(extensions='.py'):
    return []
  return input_api.RunTests(
    input_api.canned_checks.GetUnitTestsInDirectory(
      input_api, output_api, '.', [r'.+_(unit)?test\.py$']
    )
  )


def CheckMbValidate(input_api, output_api):
  if not input_api.HasAffectedFiles(
    path=['mb_config.pyl', 'mb.py', 'mb_config_expectations']
  ):
    return []
  cmd = [input_api.python3_executable, 'mb.py', 'validate']
  kwargs = {'cwd': input_api.PresubmitLocalPath()}
  return input_api.RunTests(
    [
      input_api.Command(
        name='mb_validate',
        cmd=cmd,
        kwargs=kwargs,
        message=output_api.PresubmitError,
      ),
    ]
  )
