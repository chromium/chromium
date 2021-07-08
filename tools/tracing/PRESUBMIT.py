# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tracing  unittests presubmit script.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into gcl.
"""

PRESUBMIT_VERSION = '2.0.0'
USE_PYTHON3 = True


def RunUnittests(input_api, output_api):
  presubmit_path = input_api.PresubmitLocalPath()
  return input_api.canned_checks.RunUnitTests(input_api, output_api, [
      input_api.os_path.join(presubmit_path, 'metadata_extractor_unittests.py'),
  ])


def CheckChangeOnUpload(input_api, output_api):
  return RunUnittests(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return RunUnittests(input_api, output_api)
