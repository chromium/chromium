# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit for //tools/fuzzing."""

PRESUBMIT_VERSION = '2.0.0'


def CheckUnitTests(input_api, output_api):
  if not input_api.HasAffectedFiles(extensions='.py'):
    return []
  # The runner is Linux-only.
  if input_api.is_windows or input_api.platform == 'darwin':
    return []
  return input_api.canned_checks.RunUnitTestsInDirectory(
    input_api, output_api, '.', files_to_check=[r'.+_test\.py$']
  )
