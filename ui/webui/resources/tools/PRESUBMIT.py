# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def _CheckChangeOnUploadOrCommit(input_api, output_api):
  results = []
  webui_sources = set([
      'rollup_plugin.mjs', 'generate_grd.py', 'generate_grd_test.py',
      'minify_js.py', 'minify_js_test.py', 'bundle_js.py', 'bundle_js_test.py'
  ])
  affected = input_api.AffectedFiles()
  affected_files = [input_api.os_path.basename(f.LocalPath()) for f in affected]
  if webui_sources.intersection(set(affected_files)):
    results += RunPresubmitTests(input_api, output_api)
  return results


def RunPresubmitTests(input_api, output_api):
  presubmit_path = input_api.PresubmitLocalPath()
  sources = ['generate_grd_test.py', 'minify_js_test.py', 'bundle_js_test.py']
  tests = [input_api.os_path.join(presubmit_path, s) for s in sources]
  return input_api.canned_checks.RunUnitTests(input_api, output_api, tests)


def CheckChangeOnUpload(input_api, output_api):
  return _CheckChangeOnUploadOrCommit(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CheckChangeOnUploadOrCommit(input_api, output_api)
