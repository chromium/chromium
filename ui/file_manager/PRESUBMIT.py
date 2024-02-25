# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

PRESUBMIT_VERSION = '2.0.0'

TEST_PATTERNS = [r'.+_test.py$']


class SaveSysPath():
    """Context manager to save and restore the sys.path.
  >>> with SaveSysPath(['/path/to/append/to/sys/path']):
  ...    # do stuff

  >>> # sys.path is back to original value
  """
    def __init__(self, additional_paths=None):
        self._original_path = sys.path[:]
        self._additional_paths = additional_paths

    def __enter__(self):
        if self._additional_paths:
            sys.path.extend(self._additional_paths)

    def __exit__(self, *args):
        sys.path = self._original_path


def ChecksPatchFormatted(input_api, output_api):
    return input_api.canned_checks.CheckPatchFormatted(input_api,
                                                       output_api,
                                                       check_js=True)


def ChecksUnitTests(input_api, output_api):
    # Run all unit tests under ui/file_manager/base folder.
    return input_api.canned_checks.RunUnitTestsInDirectory(
        input_api, output_api, 'base', files_to_check=TEST_PATTERNS)


def ChecksCommon(input_api, output_api):
    results = []
    cwd = input_api.PresubmitLocalPath()
    more_paths = [
        input_api.os_path.join(cwd, '..', '..', 'tools'),
        input_api.os_path.join(cwd, '..', 'chromeos'),
        input_api.os_path.join(cwd),
    ]

    with SaveSysPath(more_paths):
        from web_dev_style.presubmit_support import CheckStyle
        from styles.presubmit_support import _CheckSemanticColors
        from base.presubmit_support import _CheckNoDirectLitImport
        results += CheckStyle(input_api, output_api)
        results += _CheckSemanticColors(input_api, output_api)
        results += _CheckNoDirectLitImport(input_api, output_api)

    return results


def CheckBannedTsTags(input_api, output_api):
    results = []
    cwd = input_api.PresubmitLocalPath()
    more_paths = [
        input_api.os_path.join(cwd),
    ]
    with SaveSysPath(more_paths):
        from base.presubmit_support import _CheckBannedTsTags
        results += _CheckBannedTsTags(input_api, output_api)

    return results
