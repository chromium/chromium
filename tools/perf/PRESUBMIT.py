# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for changes affecting tools/perf/.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import os

PRESUBMIT_VERSION = '2.0.0'


def _CommonChecks(input_api, output_api, block_on_failure=False):
  """Performs common checks that vary between commit and upload.

  block_on_failure: For some failures, we would like to warn the
      user but still allow them to upload the change. However, we
      don't want them to commit code with those failures, so we
      need to block the change on commit.
  """
  commands = []
  commands.extend(
    _CheckPerfDataCurrentness(input_api, output_api, block_on_failure)
  )
  commands.extend(
    _CheckPerfJsonConfigs(input_api, output_api, block_on_failure)
  )
  commands.extend(_CheckShardMaps(input_api, output_api, block_on_failure))
  return input_api.RunTests(commands)


def CheckPresubmitUnittests(input_api, output_api):
  """Runs unittests for tools/perf/PRESUBMIT.py."""
  return input_api.canned_checks.RunUnitTestsInDirectory(
    input_api,
    output_api,
    input_api.PresubmitLocalPath(),
    [r'^PRESUBMIT_test\.py$'],
  )


def _GetPylintFilesToCheck(input_api):
  """Returns regex patterns identifying Python files to lint.

  If tools/perf/PRESUBMIT.py, pylintrc, any root Python file under
  tools/perf/, or input_api.no_diffs is set, or a Python file was deleted,
  returns None to lint all Python files under tools/perf/. Otherwise,
  scopes linting to the affected subdirectories under tools/perf/, or
  returns [] if no Python files are affected.
  """
  if input_api.no_diffs:
    return None

  local_root = input_api.PresubmitLocalPath()
  norm_root = input_api.os_path.normcase(input_api.os_path.abspath(local_root))
  this_presubmit = input_api.os_path.normcase(
    input_api.os_path.join(norm_root, 'PRESUBMIT.py')
  )
  this_pylintrc = input_api.os_path.normcase(
    input_api.os_path.join(norm_root, 'pylintrc')
  )

  affected_subdirs = set()
  has_root_py = False

  for f in input_api.AffectedFiles():
    abs_path = f.AbsoluteLocalPath()
    norm_path = input_api.os_path.normcase(input_api.os_path.abspath(abs_path))
    if norm_path in (this_presubmit, this_pylintrc):
      return None

    if not norm_path.endswith('.py'):
      continue

    # Deletions fall back to a full scan to catch broken imports across files.
    if f.Action() == 'D':
      return None

    try:
      rel_path = input_api.os_path.relpath(abs_path, local_root)
    except ValueError:
      continue

    if rel_path.startswith('..'):
      continue

    parts = rel_path.split(input_api.os_path.sep)
    if len(parts) > 1:
      affected_subdirs.add(parts[0])
    else:
      has_root_py = True

  if not affected_subdirs and not has_root_py:
    return []

  # If root Python files changed, check everything to ensure module consistency.
  if has_root_py:
    return None

  patterns = []
  for subdir in sorted(affected_subdirs):
    # Match both POSIX and Windows separators across OS regex engines.
    escaped = input_api.re.escape(subdir)
    patterns.append(rf'{escaped}(?:/|\\).*\.py$')
  return patterns


def CheckPyLint(input_api, output_api):
  files_to_check = _GetPylintFilesToCheck(input_api)
  if files_to_check == []:
    return []
  disabled_warnings = [
    'broad-exception-raised',
    'consider-using-dict-items',
    'consider-using-f-string',
    'consider-using-from-import',
    'consider-using-generator',
    'consider-using-with',
    'deprecated-module',
    'possibly-used-before-assignment',
    'redundant-u-string-prefix',
    'superfluous-parens',
    'unknown-option-value',
    'unnecessary-lambda-assignment',
    'unspecified-encoding',
    'use-dict-literal',
    'use-yield-from',
    'used-before-assignment',
    'useless-option-value',
  ]
  return input_api.RunTests(
    input_api.canned_checks.GetPylint(
      input_api,
      output_api,
      disabled_warnings=disabled_warnings,
      extra_paths_list=_GetPathsToPrepend(input_api),
      files_to_check=files_to_check,
      pylintrc='pylintrc',
      version='3.2',
    )
  )


def _GetPathsToPrepend(input_api):
  perf_dir = input_api.PresubmitLocalPath()
  chromium_src_dir = input_api.os_path.join(perf_dir, '..', '..')
  telemetry_dir = input_api.os_path.join(
    chromium_src_dir, 'third_party', 'catapult', 'telemetry'
  )
  typ_dir = input_api.os_path.join(
    chromium_src_dir, 'third_party', 'catapult', 'third_party', 'typ'
  )
  experimental_dir = input_api.os_path.join(
    chromium_src_dir, 'third_party', 'catapult', 'experimental'
  )
  tracing_dir = input_api.os_path.join(
    chromium_src_dir, 'third_party', 'catapult', 'tracing'
  )
  py_utils_dir = input_api.os_path.join(
    chromium_src_dir, 'third_party', 'catapult', 'common', 'py_utils'
  )
  android_pylib_dir = input_api.os_path.join(
    chromium_src_dir, 'build', 'android'
  )
  testing_dir = input_api.os_path.join(chromium_src_dir, 'testing')
  return [
    telemetry_dir,
    typ_dir,
    experimental_dir,
    tracing_dir,
    py_utils_dir,
    android_pylib_dir,
    testing_dir,
  ]


def _GetValidationCommands(
  input_api, output_api, script_path, extra_args=None, block_on_failure=None
):
  commands = []
  vpython = 'vpython3.bat' if input_api.is_windows else 'vpython3'
  perf_dir = input_api.PresubmitLocalPath()
  script_abs_path = input_api.os_path.join(perf_dir, script_path)
  extra_args = extra_args if extra_args else []
  # When running git cl presubmit --all this presubmit may be asked to check
  # ~500 files, leading to a command line that is over 43,000 characters.
  # This goes past the Windows 8191 character cmd.exe limit and causes cryptic
  # failures. To avoid these we break the command up into smaller pieces. The
  # non-Windows limit is chosen so that the code that splits up commands will
  # get some exercise on other platforms.
  # Depending on how long the command is on Windows the error may be:
  #     The command line is too long.
  # Or it may be:
  #     OSError: Execution failed with error: [WinError 206] The filename or
  #     extension is too long.
  # I suspect that the latter error comes from CreateProcess hitting its 32768
  # character limit.
  files_per_command = 50 if input_api.is_windows else 1000
  message_type = (
    output_api.PresubmitError
    if (block_on_failure is None or block_on_failure)
    else output_api.PresubmitPromptWarning
  )
  # Handle the case where extra_args is empty.
  for i in range(0, len(extra_args) if extra_args else 1, files_per_command):
    args = [vpython, script_abs_path] + extra_args[i : i + files_per_command]
    commands.append(
      input_api.Command(
        name='Script ' + script_path,
        cmd=args,
        kwargs={},
        message=message_type,
      )
    )
  return commands


def _RunValidationScript(
  input_api, output_api, script_path, extra_args=None, block_on_failure=None
):
  commands = _GetValidationCommands(
    input_api, output_api, script_path, extra_args, block_on_failure
  )
  return input_api.RunTests(commands)


def CheckExpectations(input_api, output_api):
  return _RunValidationScript(
    input_api,
    output_api,
    'validate_story_expectation_data',
  )


def _CheckPerfDataCurrentness(input_api, output_api, block_on_failure):
  return _GetValidationCommands(
    input_api,
    output_api,
    'generate_perf_data',
    ['--validate-only'],
    block_on_failure,
  )


def _CheckPerfJsonConfigs(input_api, output_api, block_on_failure):
  return _GetValidationCommands(
    input_api,
    output_api,
    'validate_perf_json_config',
    ['--validate-only'],
    block_on_failure,
  )


def CheckWprShaFiles(input_api, output_api):
  """Check whether the wpr sha files have matching URLs."""
  if not input_api.HasAffectedFiles(extensions='.sha1'):
    return []
  wpr_archive_shas = []
  for affected_file in input_api.AffectedFiles(include_deletes=False):
    filename = affected_file.AbsoluteLocalPath()
    if not filename.endswith('.sha1'):
      continue
    wpr_archive_shas.append(filename)
  return _RunValidationScript(
    input_api, output_api, 'validate_wpr_archives', wpr_archive_shas
  )


def _CheckShardMaps(input_api, output_api, block_on_failure):
  return _GetValidationCommands(
    input_api,
    output_api,
    'generate_perf_sharding.py',
    ['validate'],
    block_on_failure,
  )


def CheckJson(input_api, output_api):
  """Checks whether JSON files in this change can be parsed."""
  if not input_api.HasAffectedFiles(extensions='.json'):
    return []
  for affected_file in input_api.AffectedFiles(include_deletes=False):
    filename = affected_file.AbsoluteLocalPath()
    if os.path.splitext(filename)[1] != '.json':
      continue
    if (
      os.path.basename(filename) == 'perf_results.json'
      and os.path.basename(os.path.dirname(filename)) == 'speedometer2-future'
    ):
      # Intentionally invalid JSON file.
      continue
    try:
      input_api.json.load(open(filename, encoding='utf-8'))
    except ValueError:
      return [output_api.PresubmitError('Error parsing JSON in %s!' % filename)]
  return []


def CheckVersionsInSmokeTests(input_api, output_api):
  return _RunValidationScript(
    input_api,
    output_api,
    input_api.os_path.join(
      'benchmarks', 'system_health_load_tests_smoke_test.py'
    ),
  )


def CheckChangeOnUpload(input_api, output_api):
  report = []
  report.extend(_CommonChecks(input_api, output_api))
  return report


def CheckChangeOnCommit(input_api, output_api):
  report = []
  report.extend(_CommonChecks(input_api, output_api, block_on_failure=True))
  return report
