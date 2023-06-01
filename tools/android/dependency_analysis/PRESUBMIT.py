# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Top-level presubmit script for dependency_analysis.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""


def CommonChecks(input_api, output_api):
    checks = input_api.canned_checks.GetUnitTestsRecursively(
        input_api,
        output_api,
        input_api.PresubmitLocalPath(),
        files_to_check=[r'.+_unittest\.py$'],
        files_to_skip=[])

    return input_api.RunTests(checks, False)


def CheckChangeOnUpload(input_api, output_api):
    return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return CommonChecks(input_api, output_api)
