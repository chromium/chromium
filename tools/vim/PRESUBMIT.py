# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit tests for /tools/vim.

Runs Python unit tests in /tools/vim/tests on upload.
"""


def CheckChangeOnUpload(input_api, output_api):
  results = []

  # affected_files is list of files affected by this change. The paths are
  # relative to the directory containing PRESUBMIT.py.
  affected_files = [
      input_api.os_path.relpath(f, input_api.PresubmitLocalPath())
      for f in input_api.AbsoluteLocalPaths()
  ]

  # Run the chromium.ycm_extra_conf_unittest test if the YCM config file is
  # changed or if any change is affecting the tests directory. This specific
  # test requires access to 'ninja' and hasn't been tested on platforms other
  # than Linux.
  if 'chromium.ycm_extra_conf.py' in affected_files or \
      'ninja_output.py' in affected_files or \
      any([input_api.re.match(r'tests(/|\\)',f) for f in affected_files]):
    results += input_api.RunTests(
        input_api.canned_checks.GetUnitTests(
            input_api, output_api,
            ['tests/chromium.ycm_extra_conf_unittest.py']))

  return results
