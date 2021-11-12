# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

USE_PYTHON3 = True


def _CommonChecks(input_api, output_api):
  results = []

  # Run Pylint over the files in the directory.
  pylint_checks = input_api.canned_checks.GetPylint(
      input_api,
      output_api,
      version='2.7',
      # Disabling certain python3-specific warnings until the conversion
      # is complete.
      disabled_warnings=[
          'super-with-arguments',
          'raise-missing-from',
          'useless-object-inheritance',
      ],
  )
  results.extend(input_api.RunTests(pylint_checks))

  # Run the MB unittests.
  results.extend(
      input_api.canned_checks.RunUnitTestsInDirectory(input_api,
                                                      output_api,
                                                      '.',
                                                      [r'^.+_unittest\.py$'],
                                                      skip_shebang_check=True))

  # Validate the format of the mb_config.pyl file.
  cmd = [input_api.python_executable, 'mb.py', 'validate']
  kwargs = {'cwd': input_api.PresubmitLocalPath()}
  results.extend(input_api.RunTests([
      input_api.Command(name='mb_validate',
                        cmd=cmd, kwargs=kwargs,
                        message=output_api.PresubmitError)]))

  return results


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
