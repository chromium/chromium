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
from typing import Any, Callable, Iterable, List, Optional, Type

# PRESUBMIT infrastructure doesn't guarantee that the cwd() will be on
# path requiring manual path manipulation to call setup_modules.

sys.path.append('.')
import setup_modules  # pylint: disable=unused-import
sys.path.remove('.')

import chromium_src.tools.metrics.common.path_util as path_util
import chromium_src.tools.metrics.common.presubmit_util as presubmit_util
import chromium_src.tools.metrics.python_support.script_checker as script_checker
from chromium_src.tools.metrics.python_support.tests_helpers import TestableScript



class MetricsPresubmitCheckType(enum.Enum):
  # Check extracted to external scripts, treated as one unit from the
  # perspective of the cache.
  ISOLATED_CHECKS = 1
  XML_ISSUES = 2


_CACHE_DIR_PATH = os.path.join(tempfile.gettempdir(), 'metrics_presubmit_cache')

UKM_XML = 'ukm.xml'
ENUMS_XML = 'enums.xml'


def _PresubmitChecksAsTestableScripts(
    affected_files: Iterable[pathlib.Path]) -> List[TestableScript]:
  affected_files_str = [str(p) for p in affected_files]
  return [
      TestableScript.CreatePythonScript(
          pathlib.Path('tools/metrics/python_support/validate_gn.py')),
      TestableScript.CreatePythonScript(
          pathlib.Path('tools/metrics/python_support/validate_mypy.py')),
      TestableScript.CreatePythonScript(pathlib.Path(
          'tools/metrics/python_support/validate_affected_scripts.py'),
                                        flags=affected_files_str),
      TestableScript.CreatePythonScript(
          pathlib.Path('tools/metrics/metrics_python_tests.py'),
          flags=['--affected_only'] + affected_files_str,
      ),
  ]


def _ReportIsolatedChecksIssues(input_api: Type,
                                output_api: Type) -> Iterable[Any]:
  """Detects and reports issue with python scripts within tools/metrics."""
  affected_files = [f.LocalPath() for f in input_api.AffectedFiles()]
  for res in script_checker.check_scripts(
      _PresubmitChecksAsTestableScripts(affected_files),
      cwd=str(path_util.CHROMIUM_SRC_PATH),
      display_progressbar=False,
  ):
    yield output_api.PresubmitError(res.error_message())

def _ReportEnumXmlIssues(input_api: Type, output_api: Type,
                         affected_files: List[str]) -> Iterable[Any]:
  enums_changed = any(
      [input_api.basename(p) == ENUMS_XML for p in affected_files])

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
        f'reported error(s) or warning(s).\n\n{res.stdout}\n{res.stderr}')


def _ReportIsolatedChecksIssuesList(input_api: Type,
                                    output_api: Type) -> List[Any]:
  return list(_ReportIsolatedChecksIssues(input_api, output_api))


def _ReportXmlIssuesList(input_api: Type, output_api: Type) -> List[Any]:
  return list(_ReportXmlIssues(input_api, output_api))

def _ReportXmlIssues(input_api: Type, output_api: Type) -> Iterable[Any]:
  """Checks that ukm/ukm.xml is validated on changes to histograms/enums.xml."""
  affected_files = [f.LocalPath() for f in input_api.AffectedFiles()]
  ukm_xml_modified = any(
      [input_api.basename(p) == UKM_XML for p in affected_files])

  # Early return if the ukm file is changed, then the presubmit script in the
  # ukm directory would run and report the errors.
  if ukm_xml_modified:
    return

  yield from _ReportEnumXmlIssues(input_api, output_api, affected_files)


def _CheckNoManualSysPathManipulation(input_api: Any,
                                      output_api: Any) -> List[Any]:
  """Checks that no manual sys.path manipulation is done in tools/metrics."""
  results = []
  sys_path_pattern = input_api.re.compile(
      r'sys\.path\.(append|insert|extend)\(|sys\.path\s*\+?=')

  python_files = lambda f: f.LocalPath().endswith('.py')
  for affected_file in input_api.AffectedSourceFiles(python_files):
    filepath = affected_file.LocalPath()
    if 'setup_modules' in filepath or input_api.os_path.basename(
        filepath) == 'PRESUBMIT.py':
      continue
    for line_number, line in affected_file.ChangedContents():
      if sys_path_pattern.search(line):
        results.append(
            output_api.PresubmitError(
                f'{filepath}:{line_number} uses manual sys.path manipulation. '
                f'Use setup_modules instead.'))
  return results


def _CheckQuoteConsistency(input_api: Any, output_api: Any) -> List[Any]:
  """Checks that single quotes are used consistently unless double quotes
  are needed.
  """
  results = []
  cwd = input_api.PresubmitLocalPath()
  # Matches double-quoted strings (e.g. "string") while ignoring:
  # - Escaped double quotes (e.g. \").
  # - Triple double quotes (e.g. """docstrings""").
  # - Strings containing unescaped single quotes (e.g. "don't" or 'don\'t'),
  #   since double quotes are preferred in these cases to avoid escaping.
  #
  # Breakdown:
  # - (?<!\\)(?:\\\\)*: Matches an even number of backslashes before the quote,
  #   ensuring the quote itself is not escaped.
  # - (?<!")"(?!"): Matches a literal opening double quote (not part of """).
  # - (?:[^"\'\\]|\\[^\'])*: Matches string content, allowing:
  #   - Any char except double quotes, single quotes, or backslashes.
  #   - Any escape sequence except an escaped single quote (\').
  # - "(?!"): Matches the literal closing double quote.
  double_quote_pattern = input_api.re.compile(
      r'(?<!\\)(?:\\\\)*(?<!")"(?:[^"\'\\]|\\[^\'])*"(?!")')

  for affected_file in input_api.AffectedFiles(include_deletes=False):
    filepath = input_api.os_path.relpath(affected_file.AbsoluteLocalPath(), cwd)
    if filepath.endswith('.py'):
      if filepath == 'PRESUBMIT.py':
        continue
      for line_number, line in affected_file.ChangedContents():
        if double_quote_pattern.search(line):
          results.append(
              output_api.PresubmitError(
                  f'{filepath}:{line_number} uses double quotes. '
                  f'Favor single quotes unless double quotes are '
                  f'needed to avoid escapes.'))
  return results


def CheckChange(input_api: Type, output_api: Type):
  problems: List[Any] = []
  problems.extend(_CheckNoManualSysPathManipulation(input_api, output_api))
  problems.extend(_CheckQuoteConsistency(input_api, output_api))
  problems.extend(
      input_api.canned_checks.CheckPatchFormatted(input_api, output_api))
  problems.extend(
      presubmit_util.RunCheckWithCache(
          _ReportIsolatedChecksIssuesList,
          MetricsPresubmitCheckType.ISOLATED_CHECKS, input_api, output_api,
          _CACHE_DIR_PATH))
  problems.extend(
      presubmit_util.RunCheckWithCache(_ReportXmlIssuesList,
                                       MetricsPresubmitCheckType.XML_ISSUES,
                                       input_api, output_api, _CACHE_DIR_PATH))
  return problems


def CheckChangeOnUpload(input_api: Type, output_api: Type):
  return CheckChange(input_api, output_api)


def CheckChangeOnCommit(input_api: Type, output_api: Type):
  return CheckChange(input_api, output_api)
