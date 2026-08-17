# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


PRESUBMIT_VERSION = '2.0.0'


def CheckFreeze(input_api, output_api):
  return input_api.canned_checks.CheckInfraFreeze(input_api, output_api)


def CheckTests(input_api, output_api):
  return input_api.RunTests(
    input_api.canned_checks.GetUnitTestsInDirectory(
      input_api, output_api, '.', [r'.+_(unit)?test\.py$']
    )
  )


def CheckPylint(input_api, output_api):
  disabled_warnings = [
    'bad-indentation',
    'consider-using-with',
    'line-too-long',
    'missing-module-docstring',
    'singleton-comparison',
    'unspecified-encoding',
    'unused-import',
  ]
  return input_api.canned_checks.RunPylint(
    input_api,
    output_api,
    version='3.2',
    files_to_skip=['PRESUBMIT_test.py'],
    disabled_warnings=disabled_warnings,
  )


def CheckMbValidate(input_api, output_api):
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
