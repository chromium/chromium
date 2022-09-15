# Lint as: python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Top-level presubmit script for python_utils.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

USE_PYTHON3 = True
PRESUBMIT_VERSION = '2.0.0'


def CheckChange(input_api, output_api):
    # These tests contain too many Linux assumptions (pwd command, output when
    # files are missing) for them to run on Windows, so exit early.
    if input_api.is_windows:
        return []
    """Presubmit checks to run on upload and on commit of a CL."""
    checks = input_api.canned_checks.GetUnitTestsRecursively(
        input_api,
        output_api,
        input_api.PresubmitLocalPath(),
        files_to_check=[r'.+_unittest\.py$'],
        files_to_skip=[],
        run_on_python2=False,
        run_on_python3=True)

    return input_api.RunTests(checks, False)
