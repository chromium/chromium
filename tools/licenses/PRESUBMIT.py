# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'

# This line is 'magic' in that git-cl looks for it to decide whether to
# use Python3 instead of Python2 when running the code in this file.


def CheckPythonTests(input_api, output_api):
  return input_api.RunTests(
      input_api.canned_checks.GetUnitTestsInDirectory(
          input_api,
          output_api,
          input_api.PresubmitLocalPath(),
          files_to_check=[r'.+_(?:unit)?test\.py$']))
