# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for //ui."""

PRESUBMIT_VERSION = '2.0.0'


def CheckRuff(input_api, output_api):
  return input_api.canned_checks.RunRuff(input_api, output_api)


def CheckPatchFormatted(input_api, output_api):
  return input_api.canned_checks.CheckPatchFormatted(
    input_api,
    output_api,
    result_factory=output_api.PresubmitError,
    bypass_warnings=False,
  )
