# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


PRESUBMIT_VERSION = '2.0.0'


def CheckFreeze(input_api, output_api):
  return input_api.canned_checks.CheckInfraFreeze(input_api, output_api)


def CheckTests(input_api, output_api):
  return input_api.RunTests(
      input_api.canned_checks.GetUnitTestsInDirectory(input_api, output_api,
                                                      '.',
                                                      [r'.+_(unit)?test\.py$']))


def CheckPylint(input_api, output_api):
  return input_api.canned_checks.RunPylint(
      input_api,
      output_api,
      version='2.7',
      # pylint complains about Checkfreeze not being defined, its probably
      # finding a different PRESUBMIT.py. Note that this warning only
      # appears if the number of Pylint jobs is greater than one.
      files_to_skip=['PRESUBMIT_test.py'],
      # Disabling this warning because this pattern involving ToSrcRelPath
      # seems intrinsic to how mb_unittest.py is implemented.
      disabled_warnings=[
          'attribute-defined-outside-init',
      ],
  )


def CheckMbValidate(input_api, output_api):
  cmd = [input_api.python3_executable, 'mb.py', 'validate']
  kwargs = {'cwd': input_api.PresubmitLocalPath()}
  return input_api.RunTests([
      input_api.Command(name='mb_validate',
                        cmd=cmd,
                        kwargs=kwargs,
                        message=output_api.PresubmitError),
  ])
