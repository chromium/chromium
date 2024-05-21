# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs python unittests.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""


def _CommonChecks(input_api, output_api):
  results = []
  results.extend(
      input_api.canned_checks.RunPylint(input_api, output_api, version='2.6'))

  files_to_skip = ['integration_test.py']
  if input_api.change.scm != 'git':
    # The clang-format hook is being migrated for cog and until that is
    # complete, tests that rely on clang-format are unsupported on cog.
    # TODO(b/333744051): Remove this when clang-format is fully migrated.
    files_to_skip.append('.*gtest_test\.py')

  commands = []
  commands.extend(
      input_api.canned_checks.GetUnitTestsRecursively(
          input_api,
          output_api,
          input_api.os_path.join(input_api.PresubmitLocalPath()),
          files_to_check=[r'.+_test\.py$'],
          files_to_skip=files_to_skip))

  # integration_test.py uses subcommands, so we can't use the standard unit test
  # presubmit API to run it.
  commands.append(
      input_api.Command(
          name='integration_test.py',
          cmd=['integration_test.py', 'run'],
          kwargs={},
          message=output_api.PresubmitError,
      ))

  results.extend(input_api.RunTests(commands))

  return results


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
