#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Fetches per-CL code coverage metrics and uncovered lines from FindIt API."""

import argparse
import base64
import json
import os
import pathlib
import sys
import urllib.parse
from typing import Any
import requests


def make_request(url: str, auth_token: str | None = None) -> bytes | None:
  """Fetches URL data via requests with optional Bearer token auth.

  Args:
      url: The absolute HTTP/HTTPS URL to request.
      auth_token: Optional Bearer OAuth token string.

  Returns:
      The raw response bytes if successful, or None on failure.
  """
  headers = {}
  if auth_token:
    headers['Authorization'] = f'Bearer {auth_token}'
  try:
    resp = requests.get(url, headers=headers, timeout=30)
    resp.raise_for_status()
    return resp.content
  except requests.RequestException as e:
    print(f'Error fetching {url}: {e}', file=sys.stderr)
    return None


def fetch_gerrit_file_lines(
  host: str,
  change: int,
  patchset: int,
  file_path: str,
  auth_token: str | None = None,
) -> dict[str, str]:
  """Fetches base64 file content from Gerrit REST API and returns line map.

  Args:
      host: Gerrit hostname string.
      change: Gerrit CL integer number.
      patchset: Gerrit patchset integer number.
      file_path: Relative path string to the target file.
      auth_token: Optional Bearer OAuth token string.

  Returns:
      A dictionary mapping 1-based line number strings to source text lines.
  """
  encoded_path = urllib.parse.quote(file_path, safe='')
  url = (
    f'https://{host}/changes/{change}/revisions/{patchset}'
    f'/files/{encoded_path}/content'
  )
  data = make_request(url, auth_token)
  if not data:
    return {}
  try:
    decoded = base64.b64decode(data).decode('utf-8', errors='replace')
    lines = decoded.splitlines()
    return {str(num): line for num, line in enumerate(lines, start=1)}
  except Exception as e:  # pylint: disable=broad-except
    print(f'Error decoding file {file_path}: {e}', file=sys.stderr)
    return {}


def parse_percentage(val: Any) -> float | None:
  """Calculates percentage float from ratio dict or scalar value.

  Args:
      val: A coverage ratio dict or numeric string/float.

  Returns:
      The calculated coverage percentage float, or None if invalid.
  """
  if val is None:
    return None
  if isinstance(val, dict):
    covered = val.get('covered', 0)
    total = val.get('total', 0)
    if total > 0:
      return round((covered / total) * 100.0, 2)
    return 100.0
  try:
    return float(val)
  except (ValueError, TypeError):
    return None


def query_uncovered_lines(
  base_url: str,
  base_params: dict[str, str],
  path: str,
  token: str | None,
  host: str,
  change: int,
  patchset: int,
) -> dict[str, str]:
  """Queries FindIt lines API for a file and extracts uncovered lines.

  Args:
      base_url: FindIt API endpoint string.
      base_params: Base query parameters dict.
      path: File relative path string.
      token: Optional OAuth token.
      host: Gerrit host.
      change: CL number.
      patchset: Patchset number.

  Returns:
      Dictionary mapping uncovered line number strings to code text.
  """
  lines_params = {**base_params, 'type': 'lines', 'path': path}
  lines_url = f'{base_url}?{urllib.parse.urlencode(lines_params)}'
  l_bytes = make_request(lines_url, token)
  uncovered_dict = {}
  if not l_bytes:
    return uncovered_dict
  try:
    l_json = json.loads(l_bytes.decode('utf-8'))
    l_files = l_json.get('data', {}).get('files', [])
    target_f = None
    if isinstance(l_files, dict):
      target_f = l_files.get(path)
    elif isinstance(l_files, list):
      target_f = next((lf for lf in l_files if lf.get('path') == path), None)
    if target_f and 'lines' in target_f:
      file_lines = fetch_gerrit_file_lines(host, change, patchset, path, token)
      for line_info in target_f['lines']:
        if line_info.get('count') == 0:
          l_num = str(line_info.get('line'))
          uncovered_dict[l_num] = file_lines.get(l_num, '')
  except Exception as e:  # pylint: disable=broad-except
    print(f'Error processing lines for {path}: {e}', file=sys.stderr)
  return uncovered_dict


def process_files(
  files_iterable: list[dict[str, Any]],
  base_url: str,
  base_params: dict[str, str],
  token: str | None,
  host: str,
  change: int,
  patchset: int,
  target_files: set[str] | None = None,
) -> dict[str, Any]:
  """Processes coverage metrics and lines for all files in percentages report.

  Args:
      files_iterable: List of file info dicts.
      base_url: FindIt API base endpoint.
      base_params: Query params dict.
      token: Optional auth token.
      host: Gerrit host.
      change: CL number.
      patchset: Patchset number.
      target_files: Optional set of file paths to filter processing for.

  Returns:
      Results dict mapping file paths to coverage details.
  """
  metrics_keys = [
    'absolute_coverage',
    'incremental_coverage',
    'absolute_unit_tests_coverage',
    'incremental_unit_tests_coverage',
  ]
  results = {}
  for f_info in files_iterable:
    path = f_info.get('path')
    if not path or (target_files is not None and path not in target_files):
      continue
    f_metrics = {}
    low_types = []
    for k in metrics_keys:
      pct = parse_percentage(f_info.get(k))
      if pct is not None:
        f_metrics[k] = pct
        if pct < 100.0:
          low_types.append(k)
    uncovered = {}
    if low_types:
      uncovered = query_uncovered_lines(
        base_url, base_params, path, token, host, change, patchset
      )
    results[path] = {
      'metrics': f_metrics,
      'low_coverage_type': low_types,
      'uncovered_lines': uncovered,
    }
  return results


def main() -> None:
  """Parses CLI arguments, queries FindIt API, and writes coverage report."""
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--host', required=True, help='Gerrit hostname')
  parser.add_argument('--project', required=True, help='Repository name')
  parser.add_argument('--change', required=True, type=int, help='CL number')
  parser.add_argument('--patchset', required=True, type=int, help='Patchset')
  parser.add_argument(
    '--output', type=pathlib.Path, help='Output artifact file path'
  )
  parser.add_argument(
    '--files',
    nargs='*',
    help='Optional target file paths to filter coverage for',
  )
  parser.add_argument('--auth-token', help='Optional LUCI OAuth token')
  args = parser.parse_args()

  token = args.auth_token or os.environ.get('LUCI_AUTH_TOKEN')
  base_url = 'https://findit-for-me.appspot.com/coverage/api/coverage-data'
  params = {
    'host': args.host,
    'project': args.project,
    'change': str(args.change),
    'patchset': str(args.patchset),
    'type': 'percentages',
    'concise': '1',
    'format': 'json',
  }
  url = f'{base_url}?{urllib.parse.urlencode(params)}'
  resp_bytes = make_request(url, token)
  if not resp_bytes:
    print('Failed to fetch coverage percentages.', file=sys.stderr)
    sys.exit(1)

  try:
    percentages_data = json.loads(resp_bytes.decode('utf-8'))
  except Exception as e:  # pylint: disable=broad-except
    print(f'Failed to parse percentages JSON: {e}', file=sys.stderr)
    sys.exit(1)

  files_data = percentages_data.get('data', {}).get('files', [])
  if isinstance(files_data, dict):
    files_iterable = [
      {'path': k, **v} if isinstance(v, dict) else {'path': k}
      for k, v in files_data.items()
    ]
  else:
    files_iterable = files_data

  target_files = set(args.files) if args.files else None
  results = process_files(
    files_iterable,
    base_url,
    params,
    token,
    args.host,
    args.change,
    args.patchset,
    target_files,
  )

  out_str = json.dumps(results, indent=2)
  if args.output:
    args.output.write_text(out_str, encoding='utf-8')
  else:
    print(out_str)


if __name__ == '__main__':
  main()
