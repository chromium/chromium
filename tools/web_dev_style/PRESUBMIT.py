# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


PRESUBMIT_VERSION = '2.0.0'


def CheckChangeOnUpload(*args):
  return _CommonChecks(*args)


def CheckChangeOnCommit(*args):
  return _CommonChecks(*args)


def _CommonChecks(input_api, output_api):
  tests = ['test_suite.py']

  return input_api.canned_checks.RunUnitTests(input_api, output_api, tests)


def CheckEsLintConfigChanges(input_api, output_api):
  """Suggest using "git cl presubmit --files" when the global configuration
    file eslint.config.mjs file is modified. This is important because
    modifications to this file can trigger ESLint errors in any .js or .ts
    files in the repository, leading to hidden presubmit errors."""
  results = []

  eslint_filter = lambda f: input_api.FilterSourceFile(
      f, files_to_check=[r'tools/web_dev_style/eslint.config.mjs$'])
  for f in input_api.AffectedFiles(include_deletes=False,
                                   file_filter=eslint_filter):
    results.append(
        output_api.PresubmitNotifyResult(
            '%(file)s modified. Consider running \'git cl presubmit '
            '--files "*.js;*.ts"\' in order to check and fix the affected '
            'files before landing this change.' % {'file': f.LocalPath()}))
  return results
