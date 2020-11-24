# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for changes affecting tools/style_variable_generator/

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

TEST_PATTERNS = [r'.+_test.py$']
STYLE_VAR_GEN_INPUTS = [
    r'^tools[\\\/]style_variable_generator[\\\/].+\.json5$'
]


def _CommonChecks(input_api, output_api):
    results = input_api.canned_checks.RunUnitTestsInDirectory(
        input_api, output_api, '.', files_to_check=TEST_PATTERNS)
    try:
        import sys
        old_sys_path = sys.path[:]
        sys.path += [
            input_api.os_path.join(input_api.change.RepositoryRoot(), 'tools')
        ]
        import style_variable_generator.presubmit_support
        results += (
            style_variable_generator.presubmit_support.FindDeletedCSSVariables(
                input_api, output_api, STYLE_VAR_GEN_INPUTS))
    finally:
        sys.path = old_sys_path
    return results


def CheckChangeOnUpload(input_api, output_api):
    return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _CommonChecks(input_api, output_api)
