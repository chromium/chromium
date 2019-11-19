# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for changes affecting tools/perf/.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import os


def _CommonChecks(input_api, output_api, block_on_failure=False):
  """Performs common checks, which includes running pylint.

    block_on_failure: For some failures, we would like to warn the
        user but still allow them to upload the change. However, we
        don't want them to commit code with those failures, so we
        need to block the change on commit.
  """
  results = []

  results.extend(_CheckExpectations(input_api, output_api))
  results.extend(_CheckJson(input_api, output_api))
  results.extend(
      _CheckPerfDataCurrentness(input_api, output_api, block_on_failure))
  results.extend(
      _CheckPerfJsonConfigs(input_api, output_api, block_on_failure))
  results.extend(_CheckWprShaFiles(input_api, output_api))
  results.extend(_CheckShardMaps(input_api, output_api, block_on_failure))
  results.extend(_CheckVersionsInSmokeTests(input_api, output_api))
  results.extend(input_api.RunTests(input_api.canned_checks.GetPylint(
      input_api, output_api, extra_paths_list=_GetPathsToPrepend(input_api),
      pylintrc='pylintrc')))
  return results


def _GetPathsToPrepend(input_api):
  perf_dir = input_api.PresubmitLocalPath()
  chromium_src_dir = input_api.os_path.join(perf_dir, '..', '..')
  telemetry_dir = input_api.os_path.join(
      chromium_src_dir, 'third_party', 'catapult', 'telemetry')
  typ_dir = input_api.os_path.join(
       chromium_src_dir, 'third_party', 'catapult', 'third_party', 'typ')
  experimental_dir = input_api.os_path.join(
      chromium_src_dir, 'third_party', 'catapult', 'experimental')
  tracing_dir = input_api.os_path.join(
      chromium_src_dir, 'third_party', 'catapult', 'tracing')
  py_utils_dir = input_api.os_path.join(
      chromium_src_dir, 'third_party', 'catapult', 'common', 'py_utils')
  android_pylib_dir = input_api.os_path.join(
      chromium_src_dir, 'build', 'android')
  testing_dir = input_api.os_path.join(chromium_src_dir, 'testing')
  return [
      telemetry_dir,
      typ_dir,
      input_api.os_path.join(telemetry_dir, 'third_party', 'mock'),
      experimental_dir,
      tracing_dir,
      py_utils_dir,
      android_pylib_dir,
      testing_dir,
  ]


def _RunArgs(args, input_api):
  p = input_api.subprocess.Popen(args, stdout=input_api.subprocess.PIPE,
                                 stderr=input_api.subprocess.STDOUT)
  out, _ = p.communicate()
  return (out, p.returncode)

def _RunValidationScript(
    input_api,
    output_api,
    script_path,
    extra_args = None,
    block_on_failure = None):
  results = []
  vpython = 'vpython.bat' if input_api.is_windows else 'vpython'
  perf_dir = input_api.PresubmitLocalPath()
  script_abs_path = input_api.os_path.join(perf_dir, script_path)
  extra_args = extra_args if extra_args else []
  args = [vpython, script_abs_path] + extra_args
  out, return_code = _RunArgs(args, input_api)
  if return_code:
    error_msg = 'Script ' + script_path + ' failed.'
    if block_on_failure is None or block_on_failure:
      results.append(output_api.PresubmitError(
          error_msg, long_text=out))
    else:
      results.append(output_api.PresubmitPromptWarning(
          error_msg, long_text=out))
  return results

def _CheckExpectations(input_api, output_api):
  return _RunValidationScript(
      input_api,
      output_api,
      'validate_story_expectation_data',
  )

def _CheckPerfDataCurrentness(input_api, output_api, block_on_failure):
  return _RunValidationScript(
      input_api,
      output_api,
      'generate_perf_data',
      ['--validate-only'],
      block_on_failure
  )

def _CheckPerfJsonConfigs(input_api, output_api, block_on_failure):
  return _RunValidationScript(
      input_api,
      output_api,
      'validate_perf_json_config',
      ['--validate-only'],
      block_on_failure
  )

def _CheckWprShaFiles(input_api, output_api):
  """Check whether the wpr sha files have matching URLs."""
  wpr_archive_shas = []
  for affected_file in input_api.AffectedFiles(include_deletes=False):
    filename = affected_file.AbsoluteLocalPath()
    if not filename.endswith('.sha1'):
      continue
    wpr_archive_shas.append(filename)
  return _RunValidationScript(
      input_api,
      output_api,
      'validate_wpr_archives',
      wpr_archive_shas
  )

def _CheckShardMaps(input_api, output_api, block_on_failure):
  return _RunValidationScript(
      input_api,
      output_api,
      'generate_perf_sharding',
      ['validate'],
      block_on_failure
  )

def _CheckJson(input_api, output_api):
  """Checks whether JSON files in this change can be parsed."""
  for affected_file in input_api.AffectedFiles(include_deletes=False):
    filename = affected_file.AbsoluteLocalPath()
    if os.path.splitext(filename)[1] != '.json':
      continue
    try:
      input_api.json.load(open(filename))
    except ValueError:
      return [output_api.PresubmitError('Error parsing JSON in %s!' % filename)]
  return []

def _CheckVersionsInSmokeTests(input_api, output_api):
  return _RunValidationScript(
      input_api,
      output_api,
      input_api.os_path.join(
          'benchmarks', 'system_health_load_tests_smoke_test.py'),
  )

def CheckChangeOnUpload(input_api, output_api):
  report = []
  report.extend(_CommonChecks(input_api, output_api))
  return report


def CheckChangeOnCommit(input_api, output_api):
  report = []
  report.extend(_CommonChecks(input_api, output_api, block_on_failure=True))
  return report
