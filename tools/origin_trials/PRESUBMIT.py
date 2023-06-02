# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def _CommonChecks(input_api, output_api):
  results = []

  # Run Pylint over the files in the directory.
  pylint_checks = input_api.canned_checks.GetPylint(input_api,
                                                    output_api,
                                                    version='2.7')
  results.extend(input_api.RunTests(pylint_checks))

  # Run the generate_token unittests.
  results.extend(
      input_api.canned_checks.RunUnitTestsInDirectory(input_api, output_api,
                                                      '.',
                                                      [r'^.+_unittest\.py$']))

  return results


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
