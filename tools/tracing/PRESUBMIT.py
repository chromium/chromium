# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tracing  unittests presubmit script.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into gcl.
"""

PRESUBMIT_VERSION = '2.0.0'


def RunUnittests(input_api, output_api):
  results = []
  # Run Pylint over the files in the directory.
  pylint_checks = input_api.canned_checks.GetPylint(input_api,
                                                    output_api,
                                                    version='2.6')
  results.extend(input_api.RunTests(pylint_checks))

  results.extend(
      input_api.canned_checks.RunUnitTestsInDirectory(
          input_api, output_api, '.', files_to_check=[r'.+_unittest\.py$']))
  return results


def CheckChangeOnUpload(input_api, output_api):
  return RunUnittests(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return RunUnittests(input_api, output_api)
