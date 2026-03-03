# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Simple script for cleaning up stale configs from fieldtrial_testing_config.

Methodology:
  Scan for all study names that appear in fieldtrial config file,
  and removes ones that don't appear anywhere in the codebase.

  First rule out studies that are exempted from removal. Then for each source
  file in scope, look for traces of every study (by their names or involved
  feature names) using regular expressions. Matches studies are retained,
  studies that fail to be found anywhere are removed.

  The script ignores WebRTC entries as those often lead to false positives.

Usage:
  vpython3 tools/variations/cleanup_stale_fieldtrial_configs.py

Run with --help to get a complete list of options this script runs with.

If this script removes features that appear to be used in the codebase,
double-check the study or feature name for typos or case differences.
"""

from __future__ import print_function

import collections
import json
import optparse
import os
import re
import subprocess
import sys
import threading

CONFIG_PATH = 'testing/variations/fieldtrial_testing_config.json'
PRESUBMIT_SCRIPT = 'testing/variations/PRESUBMIT.py'
THREAD_COUNT = 16

# The following is a list of regexes to run against literals, and if matched,
# the literal would be counted as being used. Use this to skip removal of
# studies (and studies that depend on features) that are not visible in code.
# Eg. ChromeOS where experiments are passed from ash to platform services.
_LITERAL_SKIP_REGEX_STRINGS = [
    '^CrOSLateBoot.*',
    '^CrOSEarlyBoot.*',
    '^V8Flag_.*',
    '^WebRTC-.*',
]

_LITERAL_SKIP_REGEXES = [
    re.compile(regexp_str) for regexp_str in _LITERAL_SKIP_REGEX_STRINGS
]

_DECLARE_FEATURE_PATTERNS = [
    # Basic form: BASE_FEATURE(kMyFeature...)
    re.compile(r'BASE_FEATURE\s*\([^)]*\bk(\w+)[^)]*\)'),
    # Extra pattern for net/dns
    re.compile(r'MAKE_BASE_FEATURE_WITH_STATIC_STORAGE\s*\([^)]*\bk(\w+)[^)]*\)'
               ),
    # Quoted old-style form: BASE_FEATURE(.., "My-Feature",...)
    re.compile(r'BASE_FEATURE\s*\([^)]*"([\w-]+)"[^)]*\)'),
    # Forward declaration: BASE_DECLARE_FEATURE(kMyFeature...)
    re.compile(r'BASE_DECLARE_FEATURE\s*\([^)]*\bk(\w+)[^)]*\)'),
    # Java syntax: Flag.baseFeature("My-Feature")
    re.compile(r'Flag\.baseFeature\s*\([^)]*"([\w-]+)"[^)]*\)'),
]


def is_literal_in_skiplist(literal: str) -> bool:
  """Returns true if the literal is in the skiplist."""
  for regex in _LITERAL_SKIP_REGEXES:
    if regex.match(literal):
      print('Skipping', repr(literal), 'due to', regex)
      return True
  return False


def read_literals(studies: dict[str, list[dict]]) -> dict[str, set[str]]:
  """Returns the literals from the studies, organized per study."""
  literals = {}
  for study, configs in studies.items():
    literals[study] = {study}
    for config in configs:
      for experiment in config.get('experiments', []):
        literals[study].update(experiment.get('enable_features', []))
        literals[study].update(experiment.get('disable_features', []))
  return literals


def skip_exempted_studies(literals: dict[str, set[str]]) -> set[str]:
  """Skips exempted studies."""
  found_studies = set()
  for study, literals in literals.items():
    for literal in literals:
      if is_literal_in_skiplist(literal):
        found_studies.add(study)
        break
  return found_studies


def thread_func(thread_limiter: threading.BoundedSemaphore, file_name: str,
                literal_to_study: dict[str, set[str]],
                found_studies: set[str]) -> None:
  """Runs a task that scans a file for the presence of studies.

  Essentially for given file this function performs regular expression matching.
  For each match, it extracts the actual matched literal and looks up studies
  that are referred to by that literal. Found study is marked by moving to the
  `found_studies` set data structure.

  Args:
    thread_limiter: Thread limiter to limit number of threads used.
    file_name: File name to scan for studies.
    studies: Studies to scan for.
    found_studies: Set of studies that have been found.
  """
  thread_limiter.acquire()
  try:
    with open(file_name, 'r', encoding='utf-8', errors='ignore') as file:
      content = file.read()
      for pattern in _DECLARE_FEATURE_PATTERNS:
        for match in pattern.finditer(content):
          found_literal = match.group(1)
          for study in literal_to_study.get(found_literal, []):
            found_studies.add(study)
            print(f'Found study {study} by {pattern.pattern} for literal '
                  f'{found_literal} in {file_name}. '
                  f'Confirmed studies: {len(found_studies)}')

  finally:
    thread_limiter.release()


def find_studies_by_literals(all_files: bytes,
                             literals_by_study: dict[str, set[str]],
                             found_studies: set[str],
                             thread_limiter: threading.BoundedSemaphore):
  """Finds studies by literals.

  For each file in files, iterates over literals and checks if the literal is
  in the file. If literal matches, the study name is copied to found_studies.

  Args:
    all_files: All files to scan for studies.
    literals_by_study: Maps study to literals that are relevant to it.
    found_studies: Set of studies that have been found.
    thread_limiter: Thread limiter to limit number of threads used.
  """
  literal_to_study = invert_studies(literals_by_study)

  threads = []
  for file_name in all_files.splitlines():
    args = (thread_limiter, file_name, literal_to_study, found_studies)
    threads.append(threading.Thread(target=thread_func, args=args))

  # Start all threads, then join all threads.
  for t in threads:
    t.start()
  for t in threads:
    t.join()


def sort_find_output(find_output: bytes) -> bytes:
  """Sort output from find command.

  Args:
    find_output: Output from find command.

  Returns:
    Sorted output from find command. Files with names that make it more likely
    they are relevant to fieldtrials are first.
  """

  all_files = find_output.splitlines()

  # False comes before True when sorting, so reverse=True puts the preferred
  # filename first.
  key_re = re.compile(rb'feature|switch|flag')
  all_files.sort(key=lambda x: key_re.search(x) is not None, reverse=True)

  # Revert splitlines() to recreate the original byte-output stream.
  return b'\n'.join(all_files)


def invert_studies(studies: dict[str, set[str]]) -> dict[str, set[str]]:
  """Inverts the studies dict with literals to a literal to studies mapping."""
  literal_to_study = collections.defaultdict(set)
  for study, literals in studies.items():
    for literal in literals:
      literal_to_study[literal].add(study)
  return literal_to_study


def main():
  parser = optparse.OptionParser()
  parser.add_option('--input_path',
                    help='Path to the fieldtrial config file to clean.')
  parser.add_option('--output_path',
                    help='Path to write cleaned up fieldtrial config file.')
  parser.add_option('--thread_count',
                    type='int',
                    help='The number of threads to use for scanning.')

  opts, _ = parser.parse_args()
  input_path = os.path.expanduser(opts.input_path or CONFIG_PATH)
  output_path = os.path.expanduser(opts.output_path or CONFIG_PATH)
  thread_limiter = threading.BoundedSemaphore(opts.thread_count or THREAD_COUNT)

  with open(input_path) as fin:
    studies = json.load(fin)
  print('Loaded config from', input_path)

  # Use single find command to get all files to scan for studies.
  command = ['find', '.', '-type', 'f']
  # Ignore self-referential files and test files.
  command.extend(['-not', '-name', 'fieldtrial_testing_config.*'])
  command.extend(['-not', '-name', '*test.cc'])
  command.extend(['-not', '-name', '*test.mm'])
  command.extend(['-not', '-name', '*Test.java'])
  # Pick headers, objective-c and c++ modules.
  command.extend([
      '(', '-name', '*.h', '-o', '-name', '*.cc', '-o', '-name', '*.mm', '-o',
      '-name', '*.java', ')'
  ])

  # All relevant source files.
  all_files = sort_find_output(
      subprocess.Popen(command,
                       stdout=subprocess.PIPE,
                       stderr=subprocess.DEVNULL).stdout.read())
  print(f'Working with {len(all_files.splitlines())} source files')

  # Feature and study name literals, per study.
  literals_by_study = read_literals(studies)
  print(f'Collected {len(literals_by_study)} studies')

  found_studies = set()

  # Stage one: do not process specific studies.
  found_studies |= skip_exempted_studies(literals_by_study)

  # Stage two: find studies that have any related literal present
  # in the codebase.
  find_studies_by_literals(all_files, literals_by_study, found_studies,
                           thread_limiter)

  # Stage three: copy over studies that are found.
  clean_studies = dict()
  for study in found_studies:
    clean_studies[study] = studies[study]

  with open(output_path, 'wt') as fout:
    json.dump(clean_studies, fout)
  print('Wrote cleaned config to', output_path)

  # Run presubmit script to format config file.
  retcode = subprocess.call(['vpython3', PRESUBMIT_SCRIPT, output_path])
  if retcode != 0:
    print('Failed to format output, manually run:')
    print('vpython3', PRESUBMIT_SCRIPT, output_path)


if __name__ == '__main__':
  sys.exit(main())
