# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'


def CheckPylint(input_api, output_api):
    return input_api.canned_checks.RunPylint(
        input_api,
        output_api,
        # Otherwise it will warn on docstrings
        disabled_warnings=["pointless-string-statement"])
