# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def CheckChangeOnCommit(input_api, output_api):
  results = []

  # Run the unit tests.
  results.extend(
      input_api.canned_checks.RunUnitTestsInDirectory(input_api, output_api,
                                                      '.', [r'^.+_test\.py$']))

  return results
