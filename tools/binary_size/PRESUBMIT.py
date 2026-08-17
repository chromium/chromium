# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""binary_size presubmit script

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'


def CheckPyLint(input_api, output_api):
  output = []
  # These tools don't run on Windows so these tests don't work and give many
  # verbose and cryptic failure messages.
  if input_api.sys.platform != 'win32':
    disabled_warnings = [
      'bad-indentation',
      'cell-var-from-loop',
      'consider-using-enumerate',
      'consider-using-from-import',
      'consider-using-generator',
      'consider-using-in',
      'consider-using-with',
      'deprecated-method',
      'deprecated-module',
      'duplicate-code',
      'exec-used',
      'inconsistent-return-statements',
      'line-too-long',
      'logging-not-lazy',
      'method-cache-max-size-none',
      'missing-module-docstring',
      'possibly-used-before-assignment',
      'protected-access',
      'redundant-u-string-prefix',
      'singleton-comparison',
      'superfluous-parens',
      'undefined-variable',
      'unnecessary-lambda-assignment',
      'unnecessary-semicolon',
      'unspecified-encoding',
      'unsubscriptable-object',
      'unused-import',
      'use-dict-literal',
      'use-maxsplit-arg',
      'use-yield-from',
      'used-before-assignment',
    ]
    output.extend(
      input_api.canned_checks.RunPylint(
        input_api,
        output_api,
        disabled_warnings=disabled_warnings,
        version='3.2',
      )
    )
  return output


def CheckRunUnitTests(input_api, output_api):
  output = []
  # Linting the code is skipped on Windows because it will fail due to OS
  # differences.
  if input_api.sys.platform != 'win32':
    py_tests = input_api.canned_checks.GetUnitTestsRecursively(
      input_api,
      output_api,
      input_api.PresubmitLocalPath(),
      files_to_check=[r'.+_test\.py$'],
      files_to_skip=[],
    )
    output.extend(input_api.RunTests(py_tests, False))
  return output


def CheckPathFormatted(input_api, output_api):
  return input_api.canned_checks.CheckPatchFormatted(
    input_api, output_api, check_js=True
  )
