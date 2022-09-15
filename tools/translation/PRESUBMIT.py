# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

USE_PYTHON3 = True


def _CommonChecks(input_api, output_api):
  results = []

  # Run Pylint over the files in the directory.
  pylint_checks = input_api.canned_checks.GetPylint(input_api,
                                                    output_api,
                                                    version='2.6')
  results.extend(input_api.RunTests(pylint_checks))

  # Run unittests.
  tests = input_api.canned_checks.GetUnitTestsInDirectory(
      input_api,
      output_api,
      '.', [r'^.+_unittest\.py$'],
      run_on_python2=False,
      run_on_python3=True,
      skip_shebang_check=True)
  tests.extend(
      input_api.canned_checks.GetUnitTestsInDirectory(input_api,
                                                      output_api,
                                                      'helper',
                                                      [r'^.+_unittest\.py$'],
                                                      run_on_python2=False,
                                                      run_on_python3=True,
                                                      skip_shebang_check=True))

  results.extend(input_api.RunTests(tests))
  return results


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
