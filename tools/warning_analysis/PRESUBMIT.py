# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'


def CheckPylint(input_api, output_api):
    disabled_warnings = [
        'consider-using-with',
        'missing-module-docstring',
        'pointless-string-statement',
        'superfluous-parens',
        'unspecified-encoding',
    ]
    return input_api.canned_checks.RunPylint(
        input_api,
        output_api,
        disabled_warnings=disabled_warnings,
        version='3.2',
    )
