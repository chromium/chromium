# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'

# This line is 'magic' in that git-cl looks for it to decide whether to
# use Python3 instead of Python2 when running the code in this file.


def CheckPythonTests(input_api, output_api):
  local_path = input_api.PresubmitLocalPath()
  path = lambda *p: input_api.os_path.join(local_path, *p)
  unit_tests = [
      path('spdx_writer_test.py'),
      path('tests', 'integration_test.py'),
  ]
  return input_api.RunTests(
      input_api.canned_checks.GetUnitTests(input_api,
                                           output_api,
                                           unit_tests=unit_tests))
