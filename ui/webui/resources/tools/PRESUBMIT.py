# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

webui_sources = set([
    'bundle_js.py',
    'generate_grd.py',
    'minify_js.py',
    'rollup_plugin.mjs',
    'stylelint.py',
    'lit_template_formatter/html_utils.js',
    'lit_template_formatter/format_html.js',
    'lit_template_formatter/format_expressions.js',
    'lit_template_formatter/lit_template_formatter_test.js',
    'lit_template_formatter/main.js',
    'lit_template_formatter/process_lit_template.js',
    'lit_template_formatter/serialize_html.js',
])

webui_tests = set([
    'bundle_js_test.py',
    'generate_grd_test.py',
    'lit_template_formatter_test.py',
    'minify_js_test.py',
    'stylelint_test.py',
])

def _CheckChangeOnUploadOrCommit(input_api, output_api):
  results = []
  affected = input_api.AffectedFiles()
  affected_files = [input_api.os_path.basename(f.LocalPath()) for f in affected]
  sources = webui_sources | webui_tests
  if sources.intersection(set(affected_files)):
    results += RunPresubmitTests(input_api, output_api)
  return results


def RunPresubmitTests(input_api, output_api):
  presubmit_path = input_api.PresubmitLocalPath()
  tests = [input_api.os_path.join(presubmit_path, s) for s in webui_tests]
  return input_api.canned_checks.RunUnitTests(input_api, output_api, tests)


def CheckChangeOnUpload(input_api, output_api):
  return _CheckChangeOnUploadOrCommit(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CheckChangeOnUploadOrCommit(input_api, output_api)
