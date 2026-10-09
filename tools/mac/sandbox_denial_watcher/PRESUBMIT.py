# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for tools/mac/sandbox_denial_watcher."""

PRESUBMIT_VERSION = "2.0.0"


def CheckUnitTests(input_api, output_api):
    return input_api.canned_checks.RunUnitTestsInDirectory(
        input_api,
        output_api,
        input_api.PresubmitLocalPath(),
        files_to_check=[r"^.+_test\.py$"],
    )


def CheckPylint(input_api, output_api):
    return input_api.canned_checks.RunPylint(
        input_api,
        output_api,
        version="3.2",
    )
