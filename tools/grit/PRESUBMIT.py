# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""grit unittests presubmit script.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into gcl.
"""


def RunUnittests(input_api, output_api):
  presubmit_path = input_api.PresubmitLocalPath()
  return input_api.canned_checks.RunUnitTests(input_api, output_api, [
      input_api.os_path.join('grit', 'test_suite_all.py'),
      input_api.os_path.join(input_api.PresubmitLocalPath(),
                             'preprocess_if_expr_test.py')
  ])


def CheckChangeOnUpload(input_api, output_api):
  return RunUnittests(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return RunUnittests(input_api, output_api)
