# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for tools/android/colabutils.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'
USE_PYTHON3 = True


def CheckChange(input_api, output_api):
    """Runs python unittests in this directory."""
    return input_api.canned_checks.RunUnitTestsInDirectory(
        input_api,
        output_api,
        input_api.PresubmitLocalPath(),
        files_to_check=[r'.+_unittest\.py$'])
