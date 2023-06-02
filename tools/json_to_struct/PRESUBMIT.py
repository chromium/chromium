# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for changes affecting tools/json_to_struct/

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

ALLOWEDLIST = [r'.+_test.py$']


def CheckChangeOnUpload(input_api, output_api):
  return input_api.canned_checks.RunUnitTestsInDirectory(
      input_api, output_api, '.', files_to_check=ALLOWEDLIST)


def CheckChangeOnCommit(input_api, output_api):
  return input_api.canned_checks.RunUnitTestsInDirectory(
      input_api, output_api, '.', files_to_check=ALLOWEDLIST)
