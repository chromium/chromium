# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'
USE_PYTHON3 = True


def CheckChangeOnCommit(*args):
  return _CommonChecks(*args)


def _CommonChecks(input_api, output_api):
  cwd = input_api.PresubmitLocalPath()
  path = input_api.os_path
  files = [path.basename(f.LocalPath()) for f in input_api.AffectedFiles()]
  if files and not input_api.is_windows:
    tests = [path.join(cwd, 'download_fuzz_corpora_test.py')]
    return input_api.canned_checks.RunUnitTests(input_api, output_api, tests)

  return []
