# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for files in tools/typescript/

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'


def CheckRunTypescriptTests(input_api, output_api):
  affected = input_api.AffectedFiles()

  sources = set([
      'path_mappings.py',
      'path_utils.py',
      'tsconfig_base.json',
      'ts_library.py',
      'validate_tsconfig.py',
  ])
  affected_files = [input_api.os_path.basename(f.LocalPath()) for f in affected]
  if not sources.intersection(set(affected_files)):
    return []

  presubmit_path = input_api.PresubmitLocalPath()
  sources = ['ts_library_test.py', 'path_mappings_test.py']
  tests = [input_api.os_path.join(presubmit_path, s) for s in sources]
  return input_api.canned_checks.RunUnitTests(input_api, output_api, tests)


def CheckStyleESLint(input_api, output_api):
  results = []

  try:
    import sys
    old_sys_path = sys.path[:]
    cwd = input_api.PresubmitLocalPath()
    sys.path += [input_api.os_path.join(cwd, '..', '..', 'tools')]
    from web_dev_style import presubmit_support
    results += presubmit_support.CheckStyleESLint(input_api, output_api)
  finally:
    sys.path = old_sys_path

  return results
