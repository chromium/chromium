# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Top-level presubmit script for test_health.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

USE_PYTHON3 = True
PRESUBMIT_VERSION = '2.0.0'


def _PythonChecks(input_api, output_api):
    checks = input_api.canned_checks.GetUnitTestsRecursively(
        input_api,
        output_api,
        input_api.PresubmitLocalPath(),
        files_to_check=[r'.+_unittest\.py$'],
        files_to_skip=[],
        run_on_python2=False,
        run_on_python3=True)

    return input_api.RunTests(checks, False)


def CheckChangeOnUpload(input_api, output_api):
    """Presubmit checks to run on upload of a CL."""
    return _PythonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    """Presubmit checks to run on submission (continuous queue)."""
    # Skip presubmit on CQ since the `javalang` dependency is not available in
    # non-Android checkouts; see crbug.com/1093878 for more context.
    return []
