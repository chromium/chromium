# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def _CheckForTranslations(input_api, output_api):
  shared_keywords = ['i18n(']
  html_keywords = shared_keywords + ['$118n{']
  js_keywords = shared_keywords + ['I18nBehavior', 'loadTimeData.get']

  errors = []

  for f in input_api.AffectedFiles():
    local_path = f.LocalPath()
    # Allow translation in i18n_behavior.js.
    if local_path.endswith('i18n_behavior.js'):
      continue
    # Allow translation in the cr_components directory.
    if 'cr_components' in local_path:
      continue
    keywords = None
    if local_path.endswith('.js'):
      keywords = js_keywords
    elif local_path.endswith('.html'):
      keywords = html_keywords

    if not keywords:
      continue

    for lnum, line in f.ChangedContents():
      if any(line for keyword in keywords if keyword in line):
        errors.append("%s:%d\n%s" % (f.LocalPath(), lnum, line))

  if not errors:
    return []

  return [output_api.PresubmitError("\n".join(errors) + """

Don't embed translations directly in shared UI code. Instead, inject your
translation from the place using the shared code. For an example: see
<cr-dialog>#closeText (http://bit.ly/2eLEsqh).""")]


def _CheckSvgsOptimized(input_api, output_api):
  results = []
  try:
    import sys
    old_sys_path = sys.path[:]
    cwd = input_api.PresubmitLocalPath()
    sys.path += [input_api.os_path.join(cwd, '..', '..', '..', 'tools')]
    from resources import svgo_presubmit
    results += svgo_presubmit.CheckOptimized(input_api, output_api)
  finally:
    sys.path = old_sys_path
  return results


def _CheckWebDevStyle(input_api, output_api):
  results = []
  try:
    import sys
    old_sys_path = sys.path[:]
    cwd = input_api.PresubmitLocalPath()
    sys.path += [input_api.os_path.join(cwd, '..', '..', '..', 'tools')]
    from web_dev_style import presubmit_support
    BLACKLIST = ['ui/webui/resources/js/jstemplate_compiled.js']
    file_filter = lambda f: f.LocalPath() not in BLACKLIST
    results += presubmit_support.CheckStyle(input_api, output_api, file_filter)
  finally:
    sys.path = old_sys_path
  return results


def _CheckJsModulizer(input_api, output_api):
  affected = input_api.AffectedFiles()
  affected_files = [input_api.os_path.basename(f.LocalPath()) for f in affected]

  results = []
  if 'js_modulizer.py' in affected_files:
    presubmit_path = input_api.PresubmitLocalPath()
    sources = [input_api.os_path.join('tools', 'js_modulizer_test.py')]
    tests = [input_api.os_path.join(presubmit_path, s) for s in sources]
    results += input_api.canned_checks.RunUnitTests(
        input_api, output_api, tests)
  return results


def _CommonChecks(input_api, output_api):
  results = []
  results += _CheckForTranslations(input_api, output_api)
  results += _CheckSvgsOptimized(input_api, output_api)
  results += _CheckWebDevStyle(input_api, output_api)
  results += _CheckJsModulizer(input_api, output_api)
  results += input_api.canned_checks.CheckPatchFormatted(input_api, output_api,
                                                         check_js=True)
  return results
