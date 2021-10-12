# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for files in tools/typescript/

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

USE_PYTHON3 = True


def RunTypescriptTests(input_api, output_api):
  presubmit_path = input_api.PresubmitLocalPath()
  sources = ['ts_library_test.py']
  tests = [input_api.os_path.join(presubmit_path, s) for s in sources]
  return input_api.canned_checks.RunUnitTests(input_api,
                                              output_api,
                                              tests,
                                              skip_shebang_check=True)


def _CheckChangeOnUploadOrCommit(input_api, output_api):
  results = []
  affected = input_api.AffectedFiles()

  sources = set(['ts_library.py', 'ts_library.gni', 'tsconfig_base.json'])
  affected_files = [input_api.os_path.basename(f.LocalPath()) for f in affected]
  if sources.intersection(set(affected_files)):
    results += RunTypescriptTests(input_api, output_api)

  return results


def CheckChangeOnUpload(input_api, output_api):
  return _CheckChangeOnUploadOrCommit(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CheckChangeOnUploadOrCommit(input_api, output_api)
