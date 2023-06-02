# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'

TEST_PATTERNS = [r'.+_test.py$']
# Regex patterns which identify all source json5 files we currently use in
# production. Note these patterns can assume the file path is always in unix
# style i.e. a/b/c.
STYLE_VAR_GEN_INPUTS = [
    # Matches all json5 files which are in ui/chromeos/styles.
    r'^ui\/chromeos\/styles\/.*\.json5$'
]


def CheckCrosColorCSS(input_api, output_api):
    results = []
    try:
        import sys
        old_sys_path = sys.path[:]
        sys.path += [
            input_api.os_path.join(
                input_api.change.RepositoryRoot(), 'ui', 'chromeos')
        ]
        import styles.presubmit_support
        results += styles.presubmit_support._CheckSemanticColors(
            input_api, output_api)
        sys.path += [
            input_api.os_path.join(input_api.change.RepositoryRoot(), 'tools')
        ]
        import style_variable_generator.presubmit_support
        results += (
            style_variable_generator.presubmit_support.FindDeletedCSSVariables(
                input_api, output_api, STYLE_VAR_GEN_INPUTS))
        results += input_api.canned_checks.RunUnitTestsInDirectory(
            input_api,
            output_api,
            '.',
            files_to_check=TEST_PATTERNS)
    finally:
        sys.path = old_sys_path
    return results
