# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

webui_sources = set([
    'bundle_js.py',
    'generate_code_cache.py',
    'generate_grd.py',
    'minify_js.py',
    'rollup_plugin.mjs',
    'stylelint.py',

    # eslint_ts() sources.
    'eslint_ts.py',
    'eslint/inline_event_handler.js',
    'eslint/lit_element_invalid_interface.js',
    'eslint/lit_element_structure.js',
    'eslint/lit_element_template_structure.js',
    'eslint/lit_property_accessor.js',
    'eslint/polymer_property_class_member.js',
    'eslint/polymer_property_declare.js',
    'eslint/query_utils.js',
    'eslint/web_component_missing_deps.js',
    'webui_eslint_plugin.js',
])

webui_tests = set([
    'bundle_js_test.py',
    'eslint_ts_test.py',
    'generate_grd_test.py',
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
