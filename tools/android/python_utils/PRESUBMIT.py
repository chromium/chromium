# Lint as: python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Top-level presubmit script for python_utils.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'


def CheckChange(input_api, output_api):
    # These tests contain too many Linux assumptions (pwd command, output when
    # files are missing) for them to run on Windows, so exit early.
    if input_api.is_windows:
        return []
    """Presubmit checks to run on upload and on commit of a CL."""
    files_to_skip = []
    # Skip git tests if running on non-git workspace.
    if input_api.change.scm != 'git':
        files_to_skip.append('.+git_metadata_utils_unittest\.py$')

    checks = input_api.canned_checks.GetUnitTestsRecursively(
        input_api,
        output_api,
        input_api.PresubmitLocalPath(),
        files_to_check=[r'.+_unittest\.py$'],
        files_to_skip=files_to_skip)

    return input_api.RunTests(checks, False)
