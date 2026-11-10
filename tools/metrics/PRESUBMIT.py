# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for changes to histograms/enums.xml and python scripts.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into gcl.
"""

import enum
import os
import sys
import pathlib
import tempfile
from typing import Any, Iterable, List, Type

# PRESUBMIT infrastructure doesn't guarantee that the cwd() will be on
# path requiring manual path manipulation to call setup_modules.
sys.path.append('.')
import setup_modules  # pylint: disable=unused-import

sys.path.remove('.')

import chromium_src.tools.metrics.common.path_util as path_util
import chromium_src.tools.metrics.common.presubmit_util as presubmit_util
import chromium_src.tools.metrics.python_support.quote_checker as quote_checker
import chromium_src.tools.metrics.python_support.script_checker as script_checker
from chromium_src.tools.metrics.python_support.tests_helpers import (
  TestableScript,
)


class MetricsPresubmitCheckType(enum.Enum):
  # Check extracted to external scripts, treated as one unit from the
  # perspective of the cache.
  ISOLATED_CHECKS = 1
  XML_ISSUES = 2


_CACHE_DIR_PATH = os.path.join(tempfile.gettempdir(), 'metrics_presubmit_cache')

UKM_XML = 'ukm.xml'
ENUMS_XML = 'enums.xml'


def _PresubmitChecksAsTestableScripts(
  affected_files: Iterable[str],
) -> List[TestableScript]:
  # The affected files go through stdin: on CI every file under tools/metrics
  # is affected, which is too long for a Windows command line.
  affected_files_lines = '\n'.join(affected_files)
  return [
    TestableScript.CreatePythonScript(
      pathlib.Path('tools/metrics/python_support/validate_gn.py')
    ),
    TestableScript.CreatePythonScript(
      pathlib.Path('tools/metrics/python_support/validate_mypy.py')
    ),
    TestableScript.CreatePythonScript(
      pathlib.Path('tools/metrics/python_support/validate_affected_scripts.py'),
      stdin=affected_files_lines,
    ),
    TestableScript.CreatePythonScript(
      pathlib.Path('tools/metrics/metrics_python_tests.py'),
      # Otherwise typ uploads each test result to the presubmit's ResultDB,
      # which rejects typ's test IDs.
      flags=['--affected_only', '--disable-resultsink'],
      stdin=affected_files_lines,
    ),
  ]


def _ReportIsolatedChecksIssues(
  input_api: Type, output_api: Type
) -> Iterable[Any]:
  """Detects and reports issue with python scripts within tools/metrics."""
  if not input_api.HasAffectedFiles(extensions=('.py', '.gn')):
    return
  affected_files = [f.LocalPath() for f in input_api.AffectedFiles()]
  for res in script_checker.check_scripts(
    _PresubmitChecksAsTestableScripts(affected_files),
    cwd=str(path_util.CHROMIUM_SRC_PATH),
    display_progressbar=False,
  ):
    yield output_api.PresubmitError(res.error_message())


def _ReportEnumXmlIssues(
  input_api: Type, output_api: Type, affected_files: List[str]
) -> Iterable[Any]:
  enums_changed = any(
    [input_api.basename(p) == ENUMS_XML for p in affected_files]
  )

  if not enums_changed:
    return

  script = TestableScript(
    identifiable_name=UKM_XML,
    file_path=pathlib.Path('tools/metrics/ukm/validate_format.py'),
    cmd=[
      input_api.python3_executable,
      'tools/metrics/ukm/validate_format.py',
      '--presubmit',
    ],
  )
  failed_scripts = script_checker.check_scripts(
    [script],
    cwd=str(path_util.CHROMIUM_SRC_PATH),
    display_progressbar=False,
  )
  for res in failed_scripts:
    yield output_api.PresubmitError(
      f'{UKM_XML} does not pass format validation; run '
      f'tools/metrics/ukm/validate_format.py and fix the '
      f'reported error(s) or warning(s).\n\n{res.stdout}\n{res.stderr}'
    )


def _ReportIsolatedChecksIssuesList(
  input_api: Type, output_api: Type
) -> List[Any]:
  return list(_ReportIsolatedChecksIssues(input_api, output_api))


def _ReportXmlIssuesList(input_api: Type, output_api: Type) -> List[Any]:
  return list(_ReportXmlIssues(input_api, output_api))


def _ReportXmlIssues(input_api: Type, output_api: Type) -> Iterable[Any]:
  """Checks that ukm/ukm.xml is validated on changes to histograms/enums.xml."""
  if not input_api.HasAffectedFiles(extensions='.xml'):
    return
  affected_files = [f.LocalPath() for f in input_api.AffectedFiles()]
  ukm_xml_modified = any(
    [input_api.basename(p) == UKM_XML for p in affected_files]
  )

  # Early return if the ukm file is changed, then the presubmit script in the
  # ukm directory would run and report the errors.
  if ukm_xml_modified:
    return

  yield from _ReportEnumXmlIssues(input_api, output_api, affected_files)


def _CheckNoManualSysPathManipulation(
  input_api: Any, output_api: Any
) -> List[Any]:
  """Checks that no manual sys.path manipulation is done in tools/metrics."""
  results = []
  sys_path_pattern = input_api.re.compile(
    r'sys\.path\.(append|insert|extend)\(|sys\.path\s*\+?='
  )

  python_files = lambda f: f.LocalPath().endswith('.py')
  for affected_file in input_api.AffectedSourceFiles(python_files):
    filepath = affected_file.LocalPath()
    if (
      'setup_modules' in filepath
      or input_api.os_path.basename(filepath) == 'PRESUBMIT.py'
    ):
      continue
    for line_number, line in affected_file.ChangedContents():
      if sys_path_pattern.search(line):
        results.append(
          output_api.PresubmitError(
            f'{filepath}:{line_number} uses manual sys.path manipulation. '
            f'Use setup_modules instead.'
          )
        )
  return results


def _CheckQuoteConsistency(input_api: Any, output_api: Any) -> List[Any]:
  """Checks that single quotes are used consistently in python files.

  Double quotes are allowed only when they contain single quotes (to avoid
  escaping), e.g. "don't". Triple double quotes (docstrings) and escaped
  double quotes (e.g. \\") are ignored.
  """
  results = []
  cwd = input_api.PresubmitLocalPath()

  for affected_file in input_api.AffectedFiles(
    file_filter=lambda f: f.LocalPath().endswith('.py'),
    include_deletes=False,
  ):
    filepath = input_api.os_path.relpath(affected_file.AbsoluteLocalPath(), cwd)
    changed_lines = {
      line_number for line_number, _ in affected_file.ChangedContents()
    }
    if not changed_lines:
      continue

    file_text = '\n'.join(affected_file.NewContents())
    try:
      modified_strings = quote_checker.GetModifiedStrings(
        pathlib.Path(filepath), file_text
      )
    except Exception as e:
      results.append(
        output_api.PresubmitError(f'Failed to parse {filepath}: {e}')
      )
      continue

    for modified_string in modified_strings:
      if quote_checker.CheckQuoteConsistency(modified_string, changed_lines):
        continue
      report_line = sorted(
        list(changed_lines.intersection(modified_string.lines))
      )[0]
      results.append(
        output_api.PresubmitError(
          f'{filepath}:{report_line} uses double quotes. '
          'Favor single quotes unless double quotes are needed '
          'to avoid escapes.'
        )
      )

  return results


def CheckChange(input_api: Type, output_api: Type):
  problems: List[Any] = []
  if input_api.HasAffectedFiles(extensions='.py'):
    problems.extend(_CheckNoManualSysPathManipulation(input_api, output_api))
    problems.extend(_CheckQuoteConsistency(input_api, output_api))
  problems.extend(
    input_api.canned_checks.CheckPatchFormatted(input_api, output_api)
  )
  if input_api.HasAffectedFiles(extensions=('.py', '.gn')):
    problems.extend(
      presubmit_util.RunCheckWithCache(
        _ReportIsolatedChecksIssuesList,
        MetricsPresubmitCheckType.ISOLATED_CHECKS,
        input_api,
        output_api,
        _CACHE_DIR_PATH,
      )
    )
  if input_api.HasAffectedFiles(extensions='.xml'):
    problems.extend(
      presubmit_util.RunCheckWithCache(
        _ReportXmlIssuesList,
        MetricsPresubmitCheckType.XML_ISSUES,
        input_api,
        output_api,
        _CACHE_DIR_PATH,
      )
    )
  return problems


def CheckChangeOnUpload(input_api: Type, output_api: Type):
  return CheckChange(input_api, output_api)


def CheckChangeOnCommit(input_api: Type, output_api: Type):
  return CheckChange(input_api, output_api)
