# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

USE_PYTHON3 = False


def _CommonChecks(input_api, output_api):
  results = []

  # Run Pylint over the files in the directory.
  # TODO(crbug.com/1262279): Enable these warnings after migrating to Python3.
  disabled_warnings = ('super-with-arguments', )
  pylint_checks = input_api.canned_checks.GetPylint(
      input_api, output_api, disabled_warnings=disabled_warnings, version='2.7')
  results.extend(input_api.RunTests(pylint_checks))

  # Run the generate_token unittests.
  #TODO(https://crbug.com/1274995): Run the tests on Python3.
  results.extend(
      input_api.canned_checks.RunUnitTestsInDirectory(
          input_api,
          output_api,
          '.', [r'^.+_unittest\.py$'],
          run_on_python2=not USE_PYTHON3,
          run_on_python3=USE_PYTHON3))

  return results


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
