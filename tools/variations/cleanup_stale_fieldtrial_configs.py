# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Simple script for cleaning up stale configs from fieldtrial_testing_config.

Methodology:
  Scan for all study names that appear in fieldtrial config file,
  and removes ones that don't appear anywhere in the codebase.
  The script ignores WebRTC entries as those often lead to false positives.

Usage:
  vpython3 tools/variations/cleanup_stale_fieldtrial_configs.py

Run with --help to get a complete list of options this script runs with.

If this script removes features that appear to be used in the codebase,
double-check the study or feature name for typos or case differences.
"""

from __future__ import print_function

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
    '^CrOSLateBoot.*', '^CrOSEarlyBoot.*', '^V8Flag_.*'
]

_LITERAL_SKIP_REGEXES = [
    re.compile(regexp_str) for regexp_str in _LITERAL_SKIP_REGEX_STRINGS
]
_LITERAL_CACHE = {}

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


def is_literal_in_skiplist(literal):
  for regex in _LITERAL_SKIP_REGEXES:
    if regex.match(literal):
      print('Skipping', repr(literal), 'due to', regex)
      return True
  return False


def is_study_used(study_name: str, configs: list[dict],
                  collected_features: set[str]) -> bool:
  """Checks if a given study is used in the codebase."""
  if study_name.startswith('WebRTC-'):
    return True  # Skip webrtc studies which give false positives.

  # All features in the study, plus the study name itself.
  features = {study_name}
  for config in configs:
    for experiment in config.get('experiments', []):
      features.update(experiment.get('enable_features', []))
      features.update(experiment.get('disable_features', []))

  for feature in features:
    if feature in collected_features or is_literal_in_skiplist(feature):
      return True

  return False


def clean_up_studies_func(thread_limiter, studies_map, study_name, configs,
                          collected_features):
  """Runs a limited number of tasks and updates the map with the results.

  Args:
    thread_limited: A lock used to limit the number of active threads.
    studies_map: The map where confirmed studies are added to.
    study_name: The name of the study to check.
    configs: The configs for the given study.
    code_files: A string with the paths to all code files (cc or h files).

  Side-effect:
    This function adds the study to |studies_map| if it used.
  """
  thread_limiter.acquire()
  try:
    if is_study_used(study_name, configs, collected_features):
      studies_map[study_name] = configs
  finally:
    thread_limiter.release()


def clean_up_studies(
    studies: dict[str, list[dict]], collected_features: set[str],
    thread_limiter: threading.BoundedSemaphore) -> dict[str, list[dict]]:
  """Launches threads to clean up studies."""
  threads = []
  clean_studies = {}
  for study_name, configs in studies.items():
    args = (thread_limiter, clean_studies, study_name, configs,
            collected_features)
    threads.append(threading.Thread(target=clean_up_studies_func, args=args))
  # Start all threads, then join all threads.
  for t in threads:
    t.start()
  for t in threads:
    t.join()

  return clean_studies


def collect_features_func(thread_limiter: threading.BoundedSemaphore,
                          file_name: str, collected_features: set[str]):
  """Opens a file and scans for BASE_FEATURE declarations."""
  thread_limiter.acquire()
  try:
    with open(file_name, 'r', encoding='utf-8', errors='ignore') as file:
      content = file.read()

      for pattern in _DECLARE_FEATURE_PATTERNS:
        collected_features.update(pattern.findall(content, re.ASCII))
  finally:
    thread_limiter.release()


def collect_features(all_files: bytes,
                     thread_limiter: threading.BoundedSemaphore) -> set[str]:
  """Launches threads to collect features."""
  collected_features = set()
  threads = []
  for file_name in all_files.splitlines():
    args = (thread_limiter, file_name, collected_features)
    threads.append(threading.Thread(target=collect_features_func, args=args))
  # Start all threads, then join all threads.
  for t in threads:
    t.start()
  for t in threads:
    t.join()
  return collected_features


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
  # For all files...
  command = ['find', '.', '-type', 'f']
  # pick headers, objective-c and c++ modules.
  command.extend([
      '(', '-name', '*.h', '-o', '-name', '*.cc', '-o', '-name', '*.mm', '-o',
      '-name', '*.java', ')'
  ])

  all_files = subprocess.Popen(command,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.DEVNULL).stdout.read()

  print(f'Scanning {len(all_files.splitlines())} source files')

  # Collect all features used in the codebase, that are present in BASE_FEATURE
  # statements.
  collected_features = collect_features(all_files, thread_limiter)
  print(f'Collected {len(collected_features)} features')

  clean_studies = clean_up_studies(studies, collected_features, thread_limiter)

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
