#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Programmatic inspector for LUCI Code Coverage builds.

Responsible for:
- Fetching raw build details and step history via Buildbucket CLI (bb).
- Verifying gclient config and lookup GN args against
  language_coverage_map.json.
- Verifying coverage pipeline steps (unit & overall), HTML report
  generation, and HTML upload across languages.
- Extracting target file coverage metrics and uncovered lines.
"""

import argparse
import gzip
import gzip
import json
import os
import re
import subprocess
import sys
import tempfile
import zlib
from enum import StrEnum
from pathlib import Path
from typing import Any


class BuildStatus(StrEnum):
  """LUCI Buildbucket build and step status string constants."""

  STATUS_UNSPECIFIED = 'STATUS_UNSPECIFIED'
  SCHEDULED = 'SCHEDULED'
  STARTED = 'STARTED'
  SUCCESS = 'SUCCESS'
  FAILURE = 'FAILURE'
  INFRA_FAILURE = 'INFRA_FAILURE'
  CANCELED = 'CANCELED'


TERMINAL_BUILD_STATUSES = {
  BuildStatus.SUCCESS,
  BuildStatus.FAILURE,
  BuildStatus.INFRA_FAILURE,
  BuildStatus.CANCELED,
}


class StepStatus(StrEnum):
  """LUCI Buildbucket step status constants and diagnostic sentinels."""

  STATUS_UNSPECIFIED = 'STATUS_UNSPECIFIED'
  SCHEDULED = 'SCHEDULED'
  STARTED = 'STARTED'
  SUCCESS = 'SUCCESS'
  FAILURE = 'FAILURE'
  INFRA_FAILURE = 'INFRA_FAILURE'
  CANCELED = 'CANCELED'
  NOT_FOUND = 'NOT_FOUND'
  DELEGATED_TO_COMPILATOR = 'DELEGATED_TO_COMPILATOR'


INDENT = '  '
SRC_ROOT = Path(__file__).resolve().parents[2]


def get_supported_languages() -> list[str]:
  """Returns sorted list of keys in language_coverage_map.json."""
  map_path = Path(__file__).resolve().parent / 'language_coverage_map.json'
  with open(map_path, 'r', encoding='utf-8') as f:
    cov_map = json.load(f)
  return sorted(cov_map.keys())


def parse_build_id(build_link_or_id: str) -> str:
  """Extracts numeric build ID or converts URL to standard identifier.

  Args:
      build_link_or_id: Buildbucket build URL string or raw numeric ID.

  Returns:
      Numeric build ID string or identifier string.

  Raises:
      ValueError: If given string is not a numeric build ID or LUCI URL.
  """
  s = str(build_link_or_id).strip()
  match_id = re.search(r'\b(\d{18,20})\b', s)
  if match_id:
    return match_id.group(1)
  match_url = re.search(
    r'ci\.chromium\.org/(?:ui/)?p/([^/]+)/builders/([^/]+)/([^/]+)/(\d+)', s
  )
  if match_url:
    p, b, m, n = (
      match_url.group(1),
      match_url.group(2),
      match_url.group(3),
      match_url.group(4),
    )
    return f'{p}/{b}/{m}/{n}'
  raise ValueError(
    f'Invalid LUCI build ID or URL format: \'{build_link_or_id}\'. '
    f'Expected an 18-20 digit numeric ID or a ci.chromium.org build URL.'
  )


def run_bb_get(build_id: str) -> dict[str, Any]:
  """Runs 'bb get -A -json <build_id>' and returns parsed dict.

  Args:
      build_id: Numeric build ID or builder path to inspect.

  Returns:
      Parsed JSON dictionary containing build properties and step history.
  """
  cmd = ['bb', 'get', '-A', '-json', build_id]
  res = subprocess.run(cmd, capture_output=True, text=True, check=False)
  if res.returncode != 0:
    raise RuntimeError(
      f'Failed to fetch build {build_id} via bb get: {res.stderr}'
    )
  return json.loads(res.stdout)


def check_build_status(build_data: dict[str, Any]) -> dict[str, Any]:
  """Extracts terminal status and basic build identifiers.

  Args:
      build_data: Raw build dictionary returned by bb get.

  Returns:
      Dictionary reporting build ID, status, and terminal bool flag.
  """
  build_id = str(build_data.get('id', ''))
  status = build_data.get('status', BuildStatus.STATUS_UNSPECIFIED)
  is_terminal = status in TERMINAL_BUILD_STATUSES
  return {
    'build_id': build_id,
    'status': status,
    'terminal': is_terminal,
  }


def verify_configuration(
  build_data: dict[str, Any], language: str
) -> dict[str, Any]:
  """Verifies gclient config & GN args against language_coverage_map.json.

  Args:
      build_data: Raw build dictionary returned by bb get.
      language: Programming language string identifying target requirements.

  Returns:
      Dictionary reporting validity, step status, and explicit mismatches.

  Raises:
      ValueError: If given language is not in language_coverage_map.json.
  """
  map_path = Path(__file__).resolve().parent / 'language_coverage_map.json'
  with open(map_path, 'r', encoding='utf-8') as f:
    cov_map = json.load(f)

  if language not in cov_map:
    supported = ', '.join(sorted(cov_map.keys()))
    raise ValueError(
      f'Unsupported language \'{language}\'. Supported languages: {supported}'
    )

  lang_config = cov_map[language]
  req_gclient = lang_config.get('required_gclient_vars', {})

  in_props = build_data.get('input', {}).get('properties', {})
  steps = build_data.get('steps', [])

  def _find_lookup_gn_status(step_list: list[dict[str, Any]]) -> str:
    for s in step_list:
      s_name = s.get('name', '')
      if (
        s_name == 'lookup GN args'
        or s_name.endswith('|lookup GN args')
        or 'lookup GN args' in s_name
      ):
        return s.get('status', StepStatus.STATUS_UNSPECIFIED)
      for child_key in ('substeps', 'steps', 'children'):
        if child_key in s and isinstance(s[child_key], list):
          res = _find_lookup_gn_status(s[child_key])
          if res != StepStatus.NOT_FOUND:
            return res
    return StepStatus.NOT_FOUND

  lookup_gn_step_status = _find_lookup_gn_status(steps)

  is_orchestrator = 'orchestrator' in str(in_props.get('recipe', ''))
  if is_orchestrator and lookup_gn_step_status == StepStatus.NOT_FOUND:
    lookup_gn_step_status = StepStatus.DELEGATED_TO_COMPILATOR

  gclient_valid = True
  gclient_mismatches = []
  if req_gclient.get('checkout_clang_coverage_tools') and not is_orchestrator:
    has_clang_cov = 'use_clang_coverage' in str(
      in_props
    ) or 'checkout_clang_coverage_tools' in str(in_props)
    if not has_clang_cov:
      gclient_valid = False
      gclient_mismatches.append(
        'Missing checkout_clang_coverage_tools in gclient config.'
      )

  req_gn = lang_config.get('required_gn_args', {})
  gn_arg_mismatches = []
  gn_args = in_props.get('gn_args')
  if isinstance(gn_args, dict):
    for arg_k, expected_v in req_gn.items():
      if arg_k in gn_args and gn_args[arg_k] != expected_v:
        gn_arg_mismatches.append(
          f'{arg_k} = {gn_args[arg_k]!r} (expected {expected_v!r})'
        )
  elif not is_orchestrator:
    for arg_k, expected_v in req_gn.items():
      if expected_v is True and in_props.get(arg_k) is False:
        gn_arg_mismatches.append(f'{arg_k} = False (expected True)')

  valid = gclient_valid and len(gn_arg_mismatches) == 0
  return {
    'configuration_valid': valid,
    'lookup_gn_args_step_status': lookup_gn_step_status,
    'gn_arg_mismatches': gn_arg_mismatches,
    'gclient_config_mismatches': gclient_mismatches,
  }


def first_matching_url(
  gsutil_urls: dict[str, str | None], candidate_keys: list[str]
) -> str | None:
  """Returns the first non-empty URL string found in gsutil_urls.

  Args:
      gsutil_urls: Dictionary mapping step names to GCS URLs.
      candidate_keys: Ordered list of property key strings to test.

  Returns:
      First matching URL string or None if no valid match is found.
  """
  for key in candidate_keys:
    val = gsutil_urls.get(key)
    if val:
      return str(val)
  return None


def verify_coverage_pipeline(build_data: dict[str, Any]) -> dict[str, Any]:
  """Verifies coverage pipeline execution and artifacts across languages.

  Args:
      build_data: Raw build dictionary returned by bb get.

  Returns:
      Dictionary reporting generation status, substeps, and artifact URLs across
      C++, Java, and JavaScript coverage pipelines.
  """
  steps = build_data.get('steps', [])
  out_props = build_data.get('output', {}).get('properties', {})
  gsutil_urls = out_props.get('gsutil_urls', {})

  pipelines_config = [
    (
      'cpp_overall',
      'process clang code coverage data for overall test coverage',
      'overall',
    ),
    (
      'cpp_unit',
      'process clang code coverage data for unit test coverage',
      'unit',
    ),
    (
      'java_overall',
      'process java coverage (overall)',
      'overall',
    ),
    (
      'java_unit',
      'process java coverage (unit)',
      'unit',
    ),
    (
      'js_overall',
      'process javascript coverage (overall)',
      'overall',
    ),
  ]

  results = {}
  all_pipelines_valid = True
  build_id_str = str(build_data.get('id', ''))

  for p_key, parent_step_name, p_level in pipelines_config:
    parent_status = StepStatus.NOT_FOUND
    artifacts_upload_status = StepStatus.NOT_FOUND
    tests_processed_count = 0
    skipped_no_data = False
    child_steps = []
    recipe_messages = []
    prefix = f'{parent_step_name}|'

    for s in steps:
      s_name = s.get('name', '')
      st = s.get('status', StepStatus.STATUS_UNSPECIFIED)
      props = s.get('output', {}).get('properties', {})
      no_prof_msg = (
        'skip processing clang coverage data because no profile data collected'
      )
      if no_prof_msg in s_name:
        recipe_messages.append(
          {
            'detected_condition': no_prof_msg,
            'step_name': s_name,
          }
        )
      if 'merge errors' in props or 'Found invalid profraw files' in str(s):
        recipe_messages.append(
          {
            'detected_condition': 'Found invalid profraw files',
            'step_name': s_name,
          }
        )
      if props.get('process_coverage_data_failure'):
        recipe_messages.append(
          {
            'detected_condition': 'process_coverage_data_failure = True',
            'step_name': s_name,
          }
        )

      if s_name == parent_step_name:
        parent_status = st
        continue
      if s_name.startswith(prefix):
        sub_name = s_name[len(prefix) :]
        child_steps.append({'name': sub_name, 'status': st})
        if 'skip processing because no data is found' in sub_name:
          skipped_no_data = True
          recipe_messages.append(
            {
              'detected_condition': 'skip processing because no data is found',
              'step_name': sub_name,
            }
          )
        elif 'skip processing because no profdata was generated' in sub_name:
          recipe_messages.append(
            {
              'detected_condition': 'skip processing because no profdata was generated',
              'step_name': sub_name,
            }
          )
        if sub_name.startswith(
          (
            'generate html report',
            'Generate Java coverage metadata',
            'generate javascript html report',
            'Generate JavaScript coverage metadata',
          )
        ):
          match_count = re.search(r'in (\d+) tests', s_name)
          if match_count:
            tests_processed_count = int(match_count.group(1))
        elif sub_name.startswith('gsutil Upload coverage artifacts'):
          artifacts_upload_status = st

    if (
      parent_status == StepStatus.NOT_FOUND
      and not child_steps
      and not recipe_messages
    ):
      continue

    upload_step_name = (
      'gsutil Upload coverage artifacts (2)'
      if p_level == 'overall'
      else 'gsutil Upload coverage artifacts'
    )
    if artifacts_upload_status == StepStatus.NOT_FOUND:
      for s in steps:
        if s.get('name', '') == upload_step_name:
          artifacts_upload_status = s.get(
            'status', StepStatus.STATUS_UNSPECIFIED
          )
          break
      if artifacts_upload_status == StepStatus.NOT_FOUND:
        fallback_upload_name = (
          'gsutil Upload coverage artifacts'
          if p_level == 'overall'
          else 'gsutil Upload coverage artifacts (2)'
        )
        for s in steps:
          if s.get('name', '') == fallback_upload_name:
            artifacts_upload_status = s.get(
              'status', StepStatus.STATUS_UNSPECIFIED
            )
            break

    upload_step_alt = (
      'gsutil Upload coverage artifacts'
      if upload_step_name == 'gsutil Upload coverage artifacts (2)'
      else ''
    )
    base_gs = first_matching_url(
      gsutil_urls,
      [
        f'{parent_step_name}|{upload_step_name}',
        f'{parent_step_name}|gsutil Upload coverage artifacts',
        upload_step_name,
        upload_step_alt,
      ],
    )
    profdata_gcs_url = gsutil_urls.get(
      f'{parent_step_name}|gsutil upload artifact to GS', ''
    )
    all_json_gz_url = base_gs.rstrip('/') + '/all.json.gz' if base_gs else ''
    if (
      not all_json_gz_url
      and build_id_str
      and parent_status == StepStatus.SUCCESS
      and not os.environ.get('INSPECT_COVERAGE_NO_NETWORK')
    ):
      gsutil_py = SRC_ROOT / 'third_party' / 'depot_tools' / 'gsutil.py'
      gcs_pattern = (
        f'gs://code-coverage-data/**/{build_id_str}/{p_level}/**all.json.gz'
      )
      ls_cmd = [sys.executable, str(gsutil_py), 'ls', gcs_pattern]
      res = subprocess.run(ls_cmd, capture_output=True, text=True, check=False)
      if res.returncode == 0 and res.stdout.strip():
        all_json_gz_url = res.stdout.strip().splitlines()[0]

    pipeline_success = (
      parent_status == StepStatus.SUCCESS
      and artifacts_upload_status == StepStatus.SUCCESS
    )
    if (
      parent_status != StepStatus.NOT_FOUND
      or len(child_steps) > 0
      or len(recipe_messages) > 0
    ):
      results[p_key] = {
        'parent_step_status': parent_status,
        'artifacts_upload_status': artifacts_upload_status,
        'tests_processed_count': tests_processed_count,
        'skipped_no_data': skipped_no_data,
        'recipe_error_messages_detected': recipe_messages,
        'child_steps': child_steps,
        'merged_profdata_gcs_url': profdata_gcs_url,
        'all_json_gz_url': all_json_gz_url,
        'pipeline_success': pipeline_success,
      }
      if not pipeline_success:
        all_pipelines_valid = False

  return {
    'pipelines_checked': results,
    'all_pipelines_successful': all_pipelines_valid and len(results) > 0,
  }


def format_line_ranges(line_nums: list[int]) -> str:
  """Formats a sorted list of line numbers into compact ranges (e.g. '1-3, 5').

  Args:
      line_nums: List of integer line numbers.

  Returns:
      Comma-separated string of line numbers and ranges.
  """
  if not line_nums:
    return 'None'
  sorted_nums = sorted(set(line_nums))
  ranges = []
  start = sorted_nums[0]
  end = start
  for n in sorted_nums[1:]:
    if n == end + 1:
      end = n
    else:
      ranges.append(f'{start}-{end}' if start != end else f'{start}')
      start = n
      end = n
  ranges.append(f'{start}-{end}' if start != end else f'{start}')
  return ', '.join(ranges)


_ALL_JSON_GZ_CACHE: dict[str, dict[str, Any] | None] = {}


def download_and_parse_all_json_gz_once(
  all_json_gz_url: str, force_reload: bool = False
) -> dict[str, Any] | None:
  """Downloads, decompresses, and caches a GCS all.json.gz metadata bundle.

  Args:
      all_json_gz_url: GCS URI to the target all.json.gz bundle.
      force_reload: If True, bypasses and clears the in-memory cache for url.

  Returns:
      Parsed JSON dictionary or None if unavailable.
  """
  if not all_json_gz_url:
    return None
  if force_reload:
    _ALL_JSON_GZ_CACHE.pop(all_json_gz_url, None)
  if all_json_gz_url in _ALL_JSON_GZ_CACHE:
    return _ALL_JSON_GZ_CACHE[all_json_gz_url]

  gsutil_py = SRC_ROOT / 'third_party' / 'depot_tools' / 'gsutil.py'

  try:
    with tempfile.NamedTemporaryFile(suffix='.json.gz') as tf:
      tmp_path = tf.name
      res = subprocess.run(
        [sys.executable, str(gsutil_py), 'cp', all_json_gz_url, tmp_path],
        capture_output=True,
        check=False,
      )
      if res.returncode != 0 or not os.path.exists(tmp_path):
        _ALL_JSON_GZ_CACHE[all_json_gz_url] = None
        return None
      with open(tmp_path, 'rb') as f:
        raw = f.read()
    try:
      data = json.loads(zlib.decompress(raw))
    except Exception:
      try:
        data = json.loads(gzip.decompress(raw))
      except Exception:
        data = json.loads(raw)
    _ALL_JSON_GZ_CACHE[all_json_gz_url] = data
    return data
  except Exception:
    _ALL_JSON_GZ_CACHE[all_json_gz_url] = None
    return None


def fetch_and_parse_json_file_coverage(
  all_json_gz_url: str, clean_path: str, force_reload: bool = False
) -> dict[str, Any]:
  """Extracts per-file coverage summary from a cached GCS metadata bundle.

  Args:
      all_json_gz_url: GCS URI to the target all.json.gz metadata bundle.
      clean_path: Target source file relative path.
      force_reload: If True, bypasses and clears the in-memory cache for url.

  Returns:
      Dictionary containing coverage percentages and uncovered lines.
  """
  data = download_and_parse_all_json_gz_once(all_json_gz_url, force_reload)
  if not data:
    return {'available': False}
  try:
    files = data.get('files', []) if isinstance(data, dict) else []
    target = None
    for item in files:
      if isinstance(item, dict):
        path = item.get('path', item.get('filename', ''))
        if not path:
          continue
        if path.endswith(clean_path) or clean_path.endswith(path):
          target = item
          break
    if not target:
      return {'available': False}

    uncovered_lines = []
    for l_item in target.get('lines', []):
      if isinstance(l_item, dict) and l_item.get('count', 0) == 0:
        uncovered_lines.extend(
          range(l_item.get('first', 0), l_item.get('last', 0) + 1)
        )

    summaries_data = target.get('summaries', target.get('summary', []))
    if isinstance(summaries_data, dict):
      summaries = {}
      for k_name, k_dict in summaries_data.items():
        if (
          isinstance(k_dict, dict)
          and 'covered' in k_dict
          and ('count' in k_dict or 'total' in k_dict)
        ):
          tot = k_dict.get('count', k_dict.get('total', 0))
          summaries[k_name] = {
            'covered': k_dict.get('covered', 0),
            'total': tot,
          }
    else:
      summaries = {
        s.get('name'): s for s in summaries_data if isinstance(s, dict)
      }

    def fmt_summary(name: str, alt_name: str = '') -> str:
      s = summaries.get(name) or (summaries.get(alt_name) if alt_name else None)
      if not s:
        return 'N/A'
      cov = s.get('covered', 0)
      tot = s.get('total', s.get('count', 0))
      pct = (cov / tot * 100.0) if tot > 0 else 0.0
      return f'{cov}/{tot} ({pct:.2f}%)'

    return {
      'available': True,
      'line_coverage': fmt_summary('line', 'lines'),
      'function_coverage': fmt_summary('method', 'functions'),
      'region_coverage': fmt_summary('instruction', 'regions'),
      'branch_coverage': fmt_summary('branch', 'branches'),
      'uncovered_lines': sorted(list(set(uncovered_lines))),
    }
  except Exception:
    return {'available': False}


def extract_coverage_artifacts_for_files(
  pipeline_report: dict[str, Any],
  target_files: list[str] | None = None,
) -> dict[str, dict[str, Any]]:
  """Extracts per-file coverage metrics and uncovered lines across pipelines.

  Args:
      pipeline_report: Dictionary returned by verify_coverage_pipeline.
      target_files: Optional list of relative source file paths to inspect.

  Returns:
      Dictionary mapping each target file path to its pipeline coverage data.
  """
  files = target_files or []
  pipelines_checked = pipeline_report.get('pipelines_checked', {})
  file_results = {}

  for f_path in files:
    clean_path = f_path.lstrip('/')
    file_info = {}
    for p_type, p_details in pipelines_checked.items():
      json_gz_url = p_details.get('all_json_gz_url', '')
      if json_gz_url and p_details.get('pipeline_success'):
        parsed = fetch_and_parse_json_file_coverage(json_gz_url, clean_path)
      else:
        parsed = {'available': False}

      file_info[p_type] = parsed
    file_results[clean_path] = file_info

  return file_results


def format_file_coverage_report(
  file_coverage_data: dict[str, dict[str, Any]],
) -> str:
  """Formats extracted target file coverage data into readable text summary.

  Args:
      file_coverage_data: Dictionary mapping file paths to pipeline info.

  Returns:
      Formatted multiline report string for target files.
  """
  if not file_coverage_data:
    return ''
  lines = ['\n[Target File Coverage Extraction]']
  for f_path, p_info in file_coverage_data.items():
    lines.append(f'{INDENT}* File: {f_path}')
    for p_type, details in p_info.items():
      if not details.get('available'):
        lines.append(f'{INDENT * 2}- Pipeline \'{p_type}\': Not Available')
        continue
      line_cov = details.get('line_coverage', 'N/A')
      func_cov = details.get('function_coverage', 'N/A')
      reg_cov = details.get('region_coverage', 'N/A')
      branch_cov = details.get('branch_coverage', 'N/A')
      lines.append(f'{INDENT * 2}- Pipeline {p_type!r}:')
      lines.append(
        f'{INDENT * 3}* Line Coverage: {line_cov} | Function: {func_cov}'
        f' | Region: {reg_cov} | Branch: {branch_cov}'
      )
      uncovered = details.get('uncovered_lines', [])
      ranges_str = format_line_ranges(uncovered)
      lines.append(
        f'{INDENT * 3}* Uncovered Lines ({len(uncovered)}): {ranges_str}'
      )
  return '\n'.join(lines)


def format_inspection_report(
  combined_report: dict[str, Any], language: str
) -> str:
  """Formats combined build inspection report as readable text.

  Args:
      combined_report: Combined dictionary from all inspection phases.
      language: Programming language context string.

  Returns:
      Formatted multi-line string for human inspection.
  """
  status_report = combined_report['status_check']
  config_report = combined_report['config_verification']
  pipeline_report = combined_report['coverage_pipeline_verification']
  file_report = combined_report['target_file_coverage']

  build_id = status_report['build_id']
  status = status_report['status']
  terminal = status_report['terminal']
  config_valid = 'YES' if config_report['configuration_valid'] else 'NO'
  gn_step_status = config_report['lookup_gn_args_step_status']
  pipelines_valid = (
    'YES' if pipeline_report['all_pipelines_successful'] else 'NO'
  )

  lines = []
  lines.append('=== Build Inspection (Status, Configuration & Pipeline) ===')
  lines.append(f'Build ID: {build_id}')
  lines.append(f'Terminal Status: {status} (Terminal: {terminal})')
  if not terminal:
    lines.append(f'Build status is non-terminal ({status}).')

  lines.append(f'\n[Configuration Verification ({language})]')
  lines.append(f'{INDENT}* Configuration Valid: {config_valid}')
  lines.append(f'{INDENT}* lookup GN args step: {gn_step_status}')
  if config_report['gn_arg_mismatches']:
    for m in config_report['gn_arg_mismatches']:
      lines.append(f'{INDENT * 2}- MISMATCH: {m}')
  if config_report['gclient_config_mismatches']:
    for m in config_report['gclient_config_mismatches']:
      lines.append(f'{INDENT * 2}- MISMATCH: {m}')

  lines.append('\n[Coverage Pipeline & HTML Report Verification]')
  lines.append(f'{INDENT}* All Pipelines Successful: {pipelines_valid}')
  for p_type, details in pipeline_report['pipelines_checked'].items():
    parent_status = details['parent_step_status']
    tests_cnt = details['tests_processed_count']
    artifacts_up = details.get('artifacts_upload_status', 'N/A')
    lines.append(f'{INDENT}* Pipeline: {p_type!r}')
    lines.append(
      f'{INDENT * 2}- Parent Step: {parent_status} ({tests_cnt} tests) '
      f'| Artifacts Upload: {artifacts_up}'
    )
    if details.get('recipe_error_messages_detected'):
      for r_msg in details['recipe_error_messages_detected']:
        cond = r_msg['detected_condition']
        step_nm = r_msg['step_name']
        lines.append(
          f'{INDENT * 2}- DETECTED RECIPE CONDITION: {cond!r} (Step:'
          f' {step_nm!r})'
        )
    if details.get('skipped_no_data'):
      lines.append(
        f'{INDENT * 2}- SKIPPED: \'skip processing because no data is found\''
      )
    if not details.get('pipeline_success') and details.get('child_steps'):
      lines.append(f'{INDENT * 2}- Substep breakdown:')
      for cs in details['child_steps']:
        c_name = cs['name']
        c_status = cs['status']
        lines.append(f'{INDENT * 3}* {c_name}: {c_status}')
    if details['merged_profdata_gcs_url']:
      m_url = details['merged_profdata_gcs_url']
      lines.append(f'{INDENT * 2}- Merged Profdata GCS: {m_url}')

  formatted_file_report = format_file_coverage_report(file_report)
  if formatted_file_report:
    lines.append(formatted_file_report)

  return '\n'.join(lines)


def main() -> None:
  """Parses CLI arguments, executes inspection phases, and prints report."""
  parser = argparse.ArgumentParser(
    description='Inspect LUCI Code Coverage Build'
  )
  parser.add_argument(
    '--build',
    required=True,
    help='LUCI Buildbucket URL or numeric build ID to inspect.',
  )
  parser.add_argument(
    '--language',
    required=True,
    choices=get_supported_languages(),
    help='Programming language context (e.g. cpp, java, rust).',
  )
  parser.add_argument(
    '--json',
    action='store_true',
    help='Output results as structured JSON.',
  )
  parser.add_argument(
    '--files',
    nargs='*',
    default=[],
    help='Optional target file paths to extract coverage percentages for.',
  )
  args = parser.parse_args()

  try:
    build_id = parse_build_id(args.build)
    build_data = run_bb_get(build_id)
    status_report = check_build_status(build_data)
    config_report = verify_configuration(build_data, language=args.language)
    pipeline_report = verify_coverage_pipeline(build_data)

    file_report = {}
    if args.files:
      file_report = extract_coverage_artifacts_for_files(
        pipeline_report, args.files
      )

    combined_report = {
      'status_check': status_report,
      'config_verification': config_report,
      'coverage_pipeline_verification': pipeline_report,
      'target_file_coverage': file_report,
    }

    if args.json:
      output_text = json.dumps(combined_report, indent=2)
    else:
      output_text = format_inspection_report(combined_report, args.language)

    print(output_text)

  except Exception as e:
    print(f'ERROR: {e}', file=sys.stderr)
    sys.exit(1)


if __name__ == '__main__':
  main()
