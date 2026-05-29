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


class MetricsPresubmitCheckType(enum.Enum):
  PYTHON_ISSUES = 1
  XML_ISSUES = 2


_CACHE_DIR_PATH = os.path.join(tempfile.gettempdir(), 'metrics_presubmit_cache')








UKM_XML = 'ukm.xml'
ENUMS_XML = 'enums.xml'


def _ReportPythonIssues(input_api: Type, output_api: Type) -> Iterable[Any]:
  """Detects and reports issue with python scripts within tools/metrics."""
  affected_files = [f.LocalPath() for f in input_api.AffectedFiles()]
  py_or_build_modified = any([(input_api.basename(p).endswith('.py')
                               or input_api.basename(p) == 'BUILD.gn')
                              for p in affected_files])

  if not py_or_build_modified:
    return

  yield from presubmit_util.RunPythonScript(
      input_api, output_api, pathlib.Path('python_support/validate_gn.py'))

  yield from presubmit_util.RunPythonScript(
      input_api, output_api, pathlib.Path('python_support/validate_mypy.py'))

  yield from presubmit_util.RunPythonScript(
      input_api, output_api,
      pathlib.Path('python_support/validate_affected_scripts.py'),
      affected_files)

  yield from presubmit_util.RunPythonScript(
      input_api,
      output_api,
      pathlib.Path('metrics_python_tests.py'),
      extra_args=['--affected_only'] + affected_files)


def _ReportPythonIssuesList(input_api, output_api):
  return list(_ReportPythonIssues(input_api, output_api))


def _ReportXmlIssuesList(input_api, output_api):
  return list(_ReportXmlIssues(input_api, output_api))


def _ReportEnumXmlIssues(input_api: Type, output_api: Type,
                         affected_files: List[str]) -> Iterable[Any]:
  enums_changed = any(
      [input_api.basename(p) == ENUMS_XML for p in affected_files])

  if not enums_changed:
    return

  yield from presubmit_util.RunPythonScript(
      input_api,
      output_api,
      pathlib.Path('ukm/validate_format.py'),
      extra_args=['--presubmit'],
      custom_error_msg_fn=lambda output:
      (f'{UKM_XML} does not pass format validation; run '
       f'tools/metrics/ukm/validate_format.py and fix the '
       f'reported error(s) or warning(s).\n\n{output}'))


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


def CheckChange(input_api: Type, output_api: Type):
  problems: List[Any] = []
  problems.extend(
      presubmit_util.RunCheckWithCache(
          _ReportPythonIssuesList,
          MetricsPresubmitCheckType.PYTHON_ISSUES.value, input_api, output_api,
          _CACHE_DIR_PATH))
  problems.extend(
      presubmit_util.RunCheckWithCache(
          _ReportXmlIssuesList, MetricsPresubmitCheckType.XML_ISSUES.value,
          input_api, output_api, _CACHE_DIR_PATH))
  return problems


def CheckChangeOnUpload(input_api: Type, output_api: Type):
  return CheckChange(input_api, output_api)


def CheckChangeOnCommit(input_api: Type, output_api: Type):
  return CheckChange(input_api, output_api)
