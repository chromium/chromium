# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

USE_PYTHON3 = True

TEST_PATTERNS = [r'.+_test.py$']
STYLE_VAR_GEN_INPUTS = [r'^ui[\\/]chromeos[\\/]colors[\\/].+\.json5$']


def _CommonChecks(input_api, output_api):
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
        results = input_api.canned_checks.RunUnitTestsInDirectory(
            input_api,
            output_api,
            '.',
            files_to_check=TEST_PATTERNS,
            run_on_python2=False,
            skip_shebang_check=True)
    finally:
        sys.path = old_sys_path
    return results


def CheckChangeOnUpload(input_api, output_api):
    return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _CommonChecks(input_api, output_api)
