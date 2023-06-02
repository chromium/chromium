# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit tests for /third_party/sqlite.

Runs Python unit tests in /third_party/sqlite/scripts on upload.
"""

PRESUBMIT_VERSION = '2.0.0'

def CheckPythonUnittestsPass(input_api, output_api):
    results = []
    this_dir = input_api.PresubmitLocalPath()

    results += input_api.RunTests(
        input_api.canned_checks.GetUnitTestsInDirectory(
            input_api,
            output_api,
            input_api.os_path.join(this_dir, 'scripts'),
            files_to_check=['.*unittest.py$'],
            env=None))

    return results
