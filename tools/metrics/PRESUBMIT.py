# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for ukm/ukm.xml on changes made to histograms/enums.xml

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into gcl.
"""

import sys
from pathlib import Path

sys.path.append('.')

import setup_modules

sys.path.pop()

import chromium_src.tools.metrics.python_support.tests_helpers as tests_helpers
import chromium_src.tools.metrics.python_support.mypy_helpers as mypy_helpers
import chromium_src.tools.metrics.python_support.script_checker as script_checker

UKM_XML = 'ukm.xml'
ENUMS_XML = 'enums.xml'


_FILES_MISSING_IN_BUILD_GN_ERROR_TEMPLATE = """
There are test files that are not listed in tools/metrics/BUILD.gn
metrics_python_tests rule. Those test will not be run by CI.
Please add the missing files to BUILD.gn:
{missing_files_list}
"""


def CheckChange(input_api, output_api):
  """Checks that ukm/ukm.xml is validated on changes to histograms/enums.xml"""
  problems = []

  absolute_paths_of_affected_files = [
      f.AbsoluteLocalPath() for f in input_api.AffectedFiles()
  ]

  ukm_xml_modified = any([
      input_api.basename(p) == UKM_XML for p in absolute_paths_of_affected_files
  ])

  py_or_build_modified = any([(input_api.basename(p).endswith(".py")
                               or input_api.basename(p) == "BUILD.gn")
                              for p in absolute_paths_of_affected_files])

  if py_or_build_modified:
    missing_files = tests_helpers.validate_gn_sources('metrics_python_tests')
    if missing_files:
      files_formatted_for_build_gn = [
          f'   "//{f}",' for f in sorted(missing_files)
      ]
      missing_files_list = "\n".join(files_formatted_for_build_gn)
      problems.append(output_api.PresubmitError(
        _FILES_MISSING_IN_BUILD_GN_ERROR_TEMPLATE \
           .format(missing_files_list=missing_files_list)))

  if py_or_build_modified:
    my_py_issues = mypy_helpers.run_mypy_and_filter_irrelevant(
        input_api.PresubmitLocalPath())
    problems.extend(output_api.PresubmitError(i) for i in my_py_issues)

  scripts_to_test = tests_helpers.get_affected_testable_scripts(
      Path(p) for p in absolute_paths_of_affected_files)
  if scripts_to_test:
    print(f"Running {len(scripts_to_test)} affected scripts to check them.")
    commands_failed = script_checker.check_scripts(
        scripts_to_test,
        input_api.os_path.dirname(
            input_api.os_path.dirname(input_api.PresubmitLocalPath())))
    problems.extend([
        output_api.PresubmitError(f"Failed to run {name} (code: {code})")
        for name, code in commands_failed
    ])

  # Early return if the ukm file is changed, then the presubmit script in the
  # ukm directory would run and report the errors.
  if ukm_xml_modified:
    return problems

  enums_changed = any([
      input_api.basename(p) == ENUMS_XML
      for p in absolute_paths_of_affected_files
  ])

  # This check only applies to changes to enums.xml, so if no enums are changed,
  # then there is nothing to check and we return early with no errors.
  if not enums_changed:
    return problems

  cwd = input_api.os_path.dirname(input_api.PresubmitLocalPath())
  args = [
      input_api.python3_executable, 'metrics/ukm/validate_format.py',
      '--presubmit'
  ]
  exit_code = input_api.subprocess.call(args, cwd=cwd)

  if exit_code != 0:
    problems.append(
        output_api.PresubmitError(
            '%s does not pass format validation; run '
            '%s/ukm/validate_format.py and fix the reported error(s) or '
            'warning(s).' % (UKM_XML, input_api.PresubmitLocalPath())))

  return problems


def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)
