#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Outputs stats on the total number of Kombucha tests currently implemented.

The script outputs stats like count of total number of tests, test cases and
test files.

Example:
tools\interactive_ui_tests\print_stats.py -C out/Desktop

Finished in 10.5 seconds
Total number of tests: 104
Total number of test cases: 35
Total number of test files: 26
"""

import os
import sys
import re
import bisect
import time
import locale
import subprocess
import json
import argparse
from dataclasses import dataclass
from pathlib import Path

# Constants
REPOSITORY_ROOT = Path(__file__).parent.parent.parent.resolve()
TEST_TARGETS = '//chrome/test:*'
TEST_NAME_REGEX = re.compile(
    r"TEST(_[FP])?\(\s*'?([a-zA-Z][a-zA-Z0-9]*)'?,\s*'?" + \
        r"([a-zA-Z][a-zA-Z0-9_]*)'?",
    re.MULTILINE)
INTERATIVE_UI_TESTS_REGEX = 'RunTestSequence'


@dataclass
class TestInfo:
  filepath: str
  class_name: str
  function_name: str
  offset: int


def find_all_interactive_ui_tests_sources(out_dir):
  # Get list of sources from gn tool
  gn_path = os.path.join(REPOSITORY_ROOT, 'third_party', 'depot_tools', 'gn')
  if sys.platform.startswith('win32'):
    gn_path += '.bat'

  try:
    cmd = [gn_path, 'desc', out_dir, TEST_TARGETS, 'sources', '--format=json']
    # Set an encoding to convert the binary output to a string.
    json_output = subprocess.check_output(
        cmd, encoding=locale.getpreferredencoding())

    # Convert the sources to full paths
    targets = json.loads(json_output)
    interactive_tests_sources = []
    for target in targets:
      if 'sources' in targets[target]:
        for path in targets[target]['sources']:
          parts = path.split('/')
          source_path = os.path.join(REPOSITORY_ROOT, *parts)
          if source_path.endswith('.cc'):
            interactive_tests_sources.append(source_path)

    return interactive_tests_sources
  except subprocess.CalledProcessError as e:
    raise CommandError(e.cmd, e.returncode, e.output) from None


def find_interactive_ui_tests(filepath, interactive_tests):
  try:
    text = open(filepath, 'r', encoding='utf8', errors='ignore').read()
    # Find all the tests in the file
    test_infos = []
    for match in re.finditer(TEST_NAME_REGEX, text):
      test_infos.append(
          TestInfo(filepath, match.group(2), match.group(3), match.start()))

    # Find interaction sequence tests in the file
    test_offsets = list(map(lambda x: x.offset, test_infos))
    for match in re.finditer(INTERATIVE_UI_TESTS_REGEX, text):
      index = bisect.bisect_left(test_offsets, match.start())
      if index > 0:
        interactive_tests.append(test_infos[index - 1])
    return interactive_tests
  except IOError:
    print('Failed to open ' + filepath)

  return interactive_tests


def find_all_interactive_ui_tests(out_dir):
  # traverse through all the folders under repo root
  interactive_tests = []
  interactive_tests_sources = find_all_interactive_ui_tests_sources(out_dir)
  for filepath in interactive_tests_sources:
    find_interactive_ui_tests(filepath, interactive_tests)
  return interactive_tests


def print_stats(interactive_tests, verbose=False):
  tests = set(
      list(
          map(lambda x: f'{x.class_name}::{x.function_name}',
              interactive_tests)))
  print(f'Total number of tests: {len(tests)}')
  test_cases = set(list(map(lambda x: x.class_name, interactive_tests)))
  print(f'Total number of test cases: {len(test_cases)}')
  if verbose:
    for test_case in test_cases:
      print(test_case)
  test_files = set(list(map(lambda x: x.filepath, interactive_tests)))
  print(f'Total number of test files: {len(test_files)}')


def main():
  parser = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
  parser.add_argument('-C',
                      dest='out_dir',
                      required=True,
                      help='output directory of the build')
  args, _ = parser.parse_known_args()
  start_time = time.time()
  interactive_tests = find_all_interactive_ui_tests(args.out_dir)
  elapsed_time = time.time() - start_time
  print('Finished in %.1f seconds' % elapsed_time)
  print_stats(interactive_tests)


if __name__ == '__main__':
  main()
