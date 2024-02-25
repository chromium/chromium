#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Samples clang-tidy results from a JSON file.

Provides information about number of checks triggered and a summary of some of
the checks with links back to code search.

Usage:
tools/sample_clang_tidy_results.py out/all_findings.json
"""

import argparse
import collections
import functools
import json
import logging
import os
import random
import subprocess
import sys
from pathlib import Path
from typing import Any, Dict, List


@functools.lru_cache(maxsize=None)
def get_src_path() -> str:
  src_path = Path(__file__).parent.parent.resolve()
  if not src_path:
    raise NotFoundError(
        'Could not find checkout in any parent of the current path.')
  return src_path


@functools.lru_cache(maxsize=None)
def git_rev_parse_head(path: Path):
  if (path / '.git').exists():
    return subprocess.check_output(['git', 'rev-parse', 'HEAD'],
                                   encoding='utf-8',
                                   cwd=path).strip()
  return git_rev_parse_head(path.parent)


def convert_diag_to_cs(diag: Dict[str, Any]) -> str:
  path = diag['file_path']
  line = diag['line_number']
  name = diag['diag_name']
  replacement = '\n'.join(x['new_text'] for x in diag['replacements'])

  sha = git_rev_parse_head(get_src_path() / path)

  # https://source.chromium.org/chromium/chromium/src/+/main:apps/app_restore_service.cc
  sha_and_path = f'{sha}:{path}'
  return {
      'name':
      name,
      'path': ('https://source.chromium.org/chromium/chromium/src/+/'
               f'{sha}:{path};l={line}'),
      'replacement':
      replacement
  }


@functools.lru_cache(maxsize=None)
def is_first_party_path(path: Path) -> bool:
  if path == get_src_path():
    return True

  if path == '/':
    return False

  if (path / '.git').exists() or (path / '.gclient').exists():
    return False

  return is_first_party_path(path.parent)


def is_first_party_diag(diag: Dict[str, Any]) -> bool:
  path = diag['file_path']
  if path.startswith('out/') or path.startswith('/'):
    return False
  return is_first_party_path(get_src_path() / path)


def select_random_diags(diags: List[Dict[str, Any]], number: int) -> List[Any]:
  first_party = [x for x in diags if is_first_party_diag(x)]
  if len(first_party) <= number:
    return first_party
  return random.sample(first_party, number)


def is_diag_in_test_file(diag: Dict[str, Any]) -> bool:
  file_stem = os.path.splitext(diag['file_path'])[0]
  return (file_stem.endswith('test') or file_stem.endswith('tests')
          or '_test_' in file_stem or '_unittest_' in file_stem)


def is_diag_in_third_party(diag: Dict[str, Any]) -> bool:
  return 'third_party' in diag['file_path']


def main(argv: List[str]):
  logging.basicConfig(
      format='>> %(asctime)s: %(levelname)s: %(filename)s:%(lineno)d: '
      '%(message)s',
      level=logging.INFO,
  )

  parser = argparse.ArgumentParser(
      description=__doc__,
      formatter_class=argparse.RawDescriptionHelpFormatter,
  )
  parser.add_argument('-n',
                      '--number',
                      type=int,
                      default=30,
                      help='How many checks to sample')
  parser.add_argument('--ignore-tests',
                      action='store_true',
                      help='Filters lints in test/unittest files if specified.')
  parser.add_argument('--include-third-party',
                      action='store_true',
                      help='Includes lints in third_party if specified.')
  parser.add_argument('file', help='JSON file to parse')
  opts = parser.parse_args(argv)

  with open(opts.file) as f:
    data = json.load(f)

  print(f'Files with tidy errors: {len(data["failed_tidy_files"])}')
  print(f'Timed out files: {len(data["timed_out_src_files"])}')
  diags = data['diagnostics']

  if not opts.include_third_party:
    new_diags = [x for x in diags if not is_diag_in_third_party(x)]
    print(f'Dropped {len(diags) - len(new_diags)} diags from third_party')
    diags = new_diags

  if opts.ignore_tests:
    new_diags = [x for x in diags if not is_diag_in_test_file(x)]
    print(f'Dropped {len(diags) - len(new_diags)} diags from test files')
    diags = new_diags

  counts = collections.defaultdict(int)
  for x in diags:
    name = x['diag_name']
    counts[name] += 1

  print(f'Total number of diagnostics: {len(diags)}')
  for x in sorted(counts.keys()):
    print(f'\t{x}: {counts[x]}')
  print()

  diags = select_random_diags(diags, opts.number)
  data = [convert_diag_to_cs(x) for x in diags]
  print(f'** Sample of first-party lints: **')
  for x in data:
    print(x['path'])
    print(f'\tDiagnostic: {x["name"]}')
    print(f'\tReplacement: {x["replacement"]}')
    print()

  print('** Link summary **')
  for x in data:
    print(x['path'])


if __name__ == '__main__':
  main(sys.argv[1:])
