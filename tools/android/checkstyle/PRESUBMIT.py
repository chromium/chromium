# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'


def CheckPythonTests(input_api, output_api):
    if not input_api.HasAffectedFiles(extensions=('.py', '.xml')):
        return []
    return input_api.RunTests(
        input_api.canned_checks.GetUnitTestsInDirectory(
            input_api,
            output_api,
            input_api.PresubmitLocalPath(),
            files_to_check=[r'.+_(?:unit)?test\.py$'],
        )
    )


def CheckAllJavaStyle(input_api, output_api):
    # Run checkstyle over the entire codebase to ensure that checkstyle rolls
    # and configuration changes are tested properly.
    if not input_api.platform.startswith('linux'):
        return []
    if not input_api.HasAffectedFiles(extensions=('.py', '.xml')):
        return []
    return input_api.RunTests(
        [
            input_api.Command(
                name='checkstyle --all',
                cmd=[
                    input_api.python3_executable,
                    input_api.os_path.join(
                        input_api.PresubmitLocalPath(), 'checkstyle.py'
                    ),
                    '--all',
                ],
                kwargs={},
                message=output_api.PresubmitError,
            )
        ]
    )
