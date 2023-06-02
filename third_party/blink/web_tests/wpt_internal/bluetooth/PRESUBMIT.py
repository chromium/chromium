# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Chromium presubmit script for Web Bluetooth layout tests.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts.
"""


def CheckGeneratedFiles(input_api, output_api):
    test_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                       'generate_test.py')
    cmd_name = 'generate_test'
    cmd = [input_api.python3_executable, test_path]

    test_cmd = input_api.Command(name=cmd_name,
                                 cmd=cmd,
                                 kwargs={},
                                 message=output_api.PresubmitError)
    if input_api.verbose:
        print('Running ' + cmd_name)

    return input_api.RunTests([test_cmd])


def CheckChangeOnUpload(input_api, output_api):
    return CheckGeneratedFiles(input_api, output_api)
