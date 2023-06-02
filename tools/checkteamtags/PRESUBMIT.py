# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for checkteamtags

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API.
"""


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def _CommonChecks(input_api, output_api):
  """Does all presubmit checks for chekteamtags."""
  results = []
  results.extend(_RunUnitTests(input_api, output_api))
  results.extend(_RunPyLint(input_api, output_api))
  return results


def _RunUnitTests(input_api, output_api):
  """Runs unit tests for checkteamtags."""
  repo_root = input_api.change.RepositoryRoot()
  checkteamtags_dir = input_api.os_path.join(repo_root, 'tools',
                                             'checkteamtags')
  test_runner = input_api.os_path.join(checkteamtags_dir, 'run_tests')
  return_code = input_api.subprocess.call(
      [input_api.python3_executable, test_runner])
  if return_code:
    message = 'Checkteamtags unit tests did not all pass.'
    return [output_api.PresubmitError(message)]
  return []


def _RunPyLint(input_api, output_api):
  """Runs unit tests for checkteamtags."""
  tests = input_api.canned_checks.GetPylint(input_api,
                                            output_api,
                                            version='2.7')
  return input_api.RunTests(tests)
