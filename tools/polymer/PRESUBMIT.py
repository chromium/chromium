# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for files in tools/polymer/

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""


def RunPolymerTests(input_api, output_api):
  presubmit_path = input_api.PresubmitLocalPath()
  sources = [
    'html_to_js_test.py',
    'html_to_wrapper_test.py',
    'css_to_wrapper_test.py',
  ]
  tests = [input_api.os_path.join(presubmit_path, s) for s in sources]
  return input_api.canned_checks.RunUnitTests(input_api, output_api, tests)


_SOURCES = [
  'html_to_js.py',
  'html_to_wrapper.py',
  'css_minifier.js',
  'css_to_wrapper.py',
  'html_to_js_test.py',
  'html_to_wrapper_test.py',
  'css_to_wrapper_test.py',
]


def _CheckChangeOnUploadOrCommit(input_api, output_api):
  if not input_api.HasAffectedFiles(path=_SOURCES):
    return []
  return RunPolymerTests(input_api, output_api)


def CheckChangeOnUpload(input_api, output_api):
  return _CheckChangeOnUploadOrCommit(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CheckChangeOnUploadOrCommit(input_api, output_api)
