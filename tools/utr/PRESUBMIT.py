# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'


def CheckTests(input_api, output_api):
  return input_api.RunTests(
      input_api.canned_checks.GetUnitTestsInDirectory(input_api, output_api,
                                                      '.', [r'.+_test\.py$']))


def CheckPylint(input_api, output_api):
  return input_api.canned_checks.RunPylint(
      input_api,
      output_api,
      version='2.7',
      files_to_skip=[r'^.bundles*$'],
  )
