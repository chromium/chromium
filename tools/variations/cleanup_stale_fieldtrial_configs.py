# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Simple script for cleaning up stale configs from fieldtrial_testing_config.

Methodology:
  Scan for all study names that appear in fieldtrial config file,
  and removes ones that don't appear anywhere in the codebase.
  The script ignores WebRTC entries as those often lead to false positives.

Usage:
  vpython tools/variations/cleanup_stale_fieldtrial_configs.py

Run with --help to get a complete list of options this script runs with.

If this script removes features that appear to be used in the codebase,
double-check the study or feature name for typos or case differences.
"""

from __future__ import print_function

import json
import optparse
import os
import subprocess
import sys
import threading

CONFIG_PATH = 'testing/variations/fieldtrial_testing_config.json'
PRESUBMIT_SCRIPT = 'testing/variations/PRESUBMIT.py'
THREAD_COUNT = 16

_LITERAL_CACHE = {}


def is_literal_used(literal):
  """Check if a given string literal is used in the codebase."""
  if literal in _LITERAL_CACHE:
    return _LITERAL_CACHE[literal]

  git_grep_cmd = ('git', 'grep', '--threads', '2', '-l', '\"%s\"' % literal)
  git_grep_proc = subprocess.Popen(git_grep_cmd, stdout=subprocess.PIPE)
  # Check for >1 since fieldtrial_testing_config.json will always be a result.
  if len(git_grep_proc.stdout.read().splitlines()) > 1:
    _LITERAL_CACHE[literal] = True
    return True

  bash_find_cmd = ('bash', '-c', 'find', '.', '-type', 'f', '|', 'grep', '-E',
                   '\"\\.(h|cc)$\"', '|', 'grep', '-E',
                   '\"(/out/|/build/|/gen/)\"', '|', 'xargs', 'grep', '-l',
                   '\\\"%s\\\"' % literal)
  bash_find_proc = subprocess.Popen(bash_find_cmd, stdout=subprocess.PIPE)
  used = len(bash_find_proc.stdout.read().splitlines()) > 0
  _LITERAL_CACHE[literal] = used
  if not used:
    print('Did not find', repr(literal))

  return used


def is_study_used(study_name, configs):
  """Checks if a given study is used in the codebase."""
  if study_name.startswith('WebRTC-'):
    return True  # Skip webrtc studies which give false positives.

  if is_literal_used(study_name):
    return True
  for config in configs:
    for experiment in config.get('experiments', []):
      for feature in experiment.get('enable_features', []):
        if is_literal_used(feature):
          return True
      for feature in experiment.get('disable_features', []):
        if is_literal_used(feature):
          return True
  return False


def thread_func(thread_limiter, studies_map, study_name, configs):
  """Runs a limited number of tasks and updates the map with the results.

  Args:
    thread_limited: A lock used to limit the number of active threads.
    studies_map: The map where confirmed studies are added to.
    study_name: The name of the study to check.
    configs: The configs for the given study.

  Side-effect:
    This function adds the study to |studies_map| if it used.
  """
  thread_limiter.acquire()
  try:
    if is_study_used(study_name, configs):
      studies_map[study_name] = configs
  finally:
    thread_limiter.release()


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

  threads = []
  clean_studies = {}
  for study_name, configs in studies.items():
    args = (thread_limiter, clean_studies, study_name, configs)
    threads.append(threading.Thread(target=thread_func, args=args))

  # Start all threads, then join all threads.
  for t in threads:
    t.start()
  for t in threads:
    t.join()

  with open(output_path, 'wt') as fout:
    json.dump(clean_studies, fout)
  print('Wrote cleaned config to', output_path)

  # Run presubmit script to format config file.
  retcode = subprocess.call(['vpython', PRESUBMIT_SCRIPT, output_path])
  if retcode != 0:
    print('Failed to format output, manually run:')
    print('vpython', PRESUBMIT_SCRIPT, output_path)


if __name__ == '__main__':
  sys.exit(main())
