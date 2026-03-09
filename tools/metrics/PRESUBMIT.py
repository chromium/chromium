# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for changes to histograms/enums.xml and python scripts.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into gcl.
"""

import os
import sys
from pathlib import Path
from typing import Iterable, Any, Type, List, Dict

# PRESUBMIT infrastructure doesn't guarantee that the cwd() will be on
# path requiring manual path manipulation to call setup_modules.
# TODO(crbug.com/488351821): Consider using subprocesses to run actual
#                            test as recommended by presubmit docs:
# https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts/
sys.path.append('.')
import setup_modules

sys.path.remove('.')

import chromium_src.tools.metrics.python_support.tests_helpers as tests_helpers
import chromium_src.tools.metrics.python_support.mypy_helpers as mypy_helpers
import chromium_src.tools.metrics.python_support.script_checker as script_checker
import chromium_src.tools.metrics.python_support.dependency_solver as dependency_solver

UKM_XML = 'ukm.xml'
ENUMS_XML = 'enums.xml'


_FILES_MISSING_IN_BUILD_GN_ERROR_TEMPLATE = """
There are test files that are not listed in tools/metrics/BUILD.gn
metrics_python_tests rule. Those test will not be run by CI.
Please add the missing files to BUILD.gn:
{missing_files_list}
"""


def _ThisDirPath(input_api: Type) -> Path:
  return Path(input_api.PresubmitLocalPath())


def _ChromiumSrcPath(input_api: Type) -> Path:
  return _ThisDirPath(input_api).joinpath('..').joinpath('..').resolve()


def _RunSelectedTests(
    input_api: Type, output_api: Type,
    test_scripts: Iterable[tests_helpers.TestableScript]) -> Iterable[Any]:
  """Executes the tests_scripts using typ"""
  test_to_run_list = "\n * ".join([''] +
                                  [str(t.file_path) for t in test_scripts])
  print(f"Executing following tests in tools/metrics: {test_to_run_list}")
  return input_api.RunTests([
      input_api.Command(name=t.file_path,
                        cmd=t.cmd,
                        kwargs={'cwd': _ChromiumSrcPath(input_api)},
                        message=output_api.PresubmitError) for t in test_scripts
  ])


def _ReportMissingBuildFileReferences(output_api: Type) -> Iterable[Any]:
  """Reports issues with files missing in BUILD.gn tests list"""
  missing_files = tests_helpers.validate_gn_sources('metrics_python_tests')
  if not missing_files:
    return []
  files_formatted_for_build_gn = [f'   "//{f}",' for f in sorted(missing_files)]
  missing_files_list = "\n".join(files_formatted_for_build_gn)
  return [
      output_api.PresubmitError(
          _FILES_MISSING_IN_BUILD_GN_ERROR_TEMPLATE.format(
              missing_files_list=missing_files_list))
  ]


def _ReportMyPyErrors(input_api: Type, output_api: Type) -> Iterable[Any]:
  my_py_issues = mypy_helpers.run_mypy_and_filter_irrelevant(
      input_api.PresubmitLocalPath())
  return [output_api.PresubmitError(i) for i in my_py_issues]


def _ReportIssuesWithScripts(input_api: Type, output_api: Type,
                             affected_files: Iterable[str],
                             deps_graph: Dict[str, List[str]]) -> Iterable[Any]:
  scripts_to_test = tests_helpers.get_affected_testable_scripts(
      set(Path(p) for p in affected_files), deps_graph)

  if not scripts_to_test:
    return []

  print(f"Running {len(scripts_to_test)} affected scripts to check them.")
  commands_failed = script_checker.check_scripts(
      scripts_to_test, cwd=str(_ChromiumSrcPath(input_api)))
  return [
      output_api.PresubmitError(res.error_message()) for res in commands_failed
  ]


def _ReportIssuesWithTests(input_api: Type, output_api: Type,
                           affected_files: List[str],
                           deps_graph: Dict[str, List[str]]) -> Iterable[Any]:
  tests_to_run = tests_helpers.get_affected_tests(
      set(Path(p) for p in affected_files), deps_graph)

  if not tests_to_run:
    return []

  return _RunSelectedTests(input_api, output_api, tests_to_run)


def _ReportPythonIssues(input_api: Type, output_api: Type) -> Iterable[Any]:
  """Detects and reports issue with python scripts within tools/metrics."""
  absolute_paths_of_affected_files = [
      f.AbsoluteLocalPath() for f in input_api.AffectedFiles()
  ]
  py_or_build_modified = any([(input_api.basename(p).endswith(".py")
                               or input_api.basename(p) == "BUILD.gn")
                              for p in absolute_paths_of_affected_files])

  if not py_or_build_modified:
    return

  deps_graph = dependency_solver.scan_directory_dependencies(
      input_api.PresubmitLocalPath(),
      report_relative_to=str(_ChromiumSrcPath(input_api)))

  yield from _ReportMissingBuildFileReferences(output_api)
  yield from _ReportMyPyErrors(input_api, output_api)
  yield from _ReportIssuesWithScripts(input_api, output_api,
                                      absolute_paths_of_affected_files,
                                      deps_graph)
  yield from _ReportIssuesWithTests(input_api, output_api,
                                    absolute_paths_of_affected_files,
                                    deps_graph)


def _ReportEnumXmlIssues(input_api: Type, output_api: Type,
                         affected_files: List[str]) -> Iterable[Any]:
  enums_changed = any(
      [input_api.basename(p) == ENUMS_XML for p in affected_files])

  if not enums_changed:
    return

  testable_script = tests_helpers.TestableScript.CreatePythonScript(
      Path('tools/metrics/ukm/validate_format.py'), ['--presubmit'])
  commands_failed = script_checker.check_scripts(
      [testable_script], cwd=str(_ChromiumSrcPath(input_api)))

  if not commands_failed:
    return

  for res in commands_failed:
    yield output_api.PresubmitError(
        f'{UKM_XML} does not pass format validation; run '
        f'{input_api.PresubmitLocalPath()}/ukm/validate_format.py and fix the '
        f'reported error(s) or warning(s).\n\n{res.error_message()}')


def _ReportXmlIssues(input_api: Type, output_api: Type) -> Iterable[Any]:
  """Checks that ukm/ukm.xml is validated on changes to histograms/enums.xml."""
  absolute_paths_of_affected_files = [
      f.AbsoluteLocalPath() for f in input_api.AffectedFiles()
  ]
  ukm_xml_modified = any([
      input_api.basename(p) == UKM_XML for p in absolute_paths_of_affected_files
  ])

  # Early return if the ukm file is changed, then the presubmit script in the
  # ukm directory would run and report the errors.
  if ukm_xml_modified:
    return

  yield from _ReportEnumXmlIssues(input_api, output_api,
                                  absolute_paths_of_affected_files)


def CheckChange(input_api: Type, output_api: Type):
  problems: List[Any] = []
  problems.extend(_ReportPythonIssues(input_api, output_api))
  problems.extend(_ReportXmlIssues(input_api, output_api))
  return problems


def CheckChangeOnUpload(input_api: Type, output_api: Type):
  return CheckChange(input_api, output_api)


def CheckChangeOnCommit(input_api: Type, output_api: Type):
  return CheckChange(input_api, output_api)
