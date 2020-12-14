# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for files in ui/file_manager/base/tools/

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""


def RunTests(input_api, output_api):
    presubmit_path = input_api.PresubmitLocalPath()
    sources = ['modules_test.py']
    tests = [input_api.os_path.join(presubmit_path, s) for s in sources]
    return input_api.canned_checks.RunUnitTests(input_api, output_api, tests)


def _CheckChangeOnUploadOrCommit(input_api, output_api):
    results = []
    affected = input_api.AffectedFiles()

    def is_in_current_folder(f):
      dirname = input_api.os_path.dirname(f.LocalPath())
      return dirname == u'ui/file_manager/base/tools'

    # Check if any of the modified files is in 'ui/file_manager/base/tools'.
    if any([is_in_current_folder(f) for f in affected]):
        results += RunTests(input_api, output_api)

    return results


def CheckChangeOnUpload(input_api, output_api):
    return _CheckChangeOnUploadOrCommit(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _CheckChangeOnUploadOrCommit(input_api, output_api)
