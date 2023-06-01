# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit tests for /tools/android/infobar_deprecation.

Runs Python unit tests in /tools/android/infobar_deprecation on upload.
"""


def CheckChangeOnUpload(input_api, output_api):
  result = []
  result.extend(
      input_api.canned_checks.RunUnitTests(input_api, output_api,
                                           ['./infobar_deprecation_test.py']))
  return result
