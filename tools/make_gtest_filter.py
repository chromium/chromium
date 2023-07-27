#!/usr/bin/env python
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Reads lines from files or stdin and identifies C++ tests.

Outputs a filter that can be used with --gtest_filter or a filter file to
run only the tests identified.

Usage:

Outputs filter for all test fixtures in a directory. --class-only avoids an
overly long filter string.
$ cat components/mycomp/**test.cc | make_gtest_filter.py --class-only

Outputs filter for all tests in a file.
$ make_gtest_filter.py ./myfile_unittest.cc

Outputs filter for only test at line 123
$ make_gtest_filter.py --line=123 ./myfile_unittest.cc

Formats output as a GTest filter file.
$ make_gtest_filter.py ./myfile_unittest.cc --as-filter-file

Use a JSON failure summary as the input.
$ make_gtest_filter.py summary.json --from-failure-summary

Elide the filter list using wildcards when possible.
$ make_gtest_filter.py summary.json --from-failure-summary --wildcard-compress
"""

from __future__ import print_function

import argparse
import collections
import fileinput
import json
import re
import sys


class TrieNode:
  def __init__(self):
    # The number of strings which terminated on or underneath this node.
    self.num_strings = 0

    # The prefix subtries which follow |this|, keyed by their next character.
    self.children = {}


def PascalCaseSplit(input_string):
  current_term = []
  prev_char = ''

  for current_char in input_string:
    is_boundary = prev_char != '' and \
                  ((current_char.isupper() and prev_char.islower()) or \
                   (current_char.isalpha() != prev_char.isalpha()) or \
                   (current_char.isalnum() != prev_char.isalnum()))
    prev_char = current_char

    if is_boundary:
      yield ''.join(current_term)
      current_term = []

    current_term.append(current_char)

  if len(current_term) > 0:
    yield ''.join(current_term)


def TrieInsert(trie, value):
  """Inserts the characters of 'value' into a trie, with every edge representing
  a single character. An empty child set indicates end-of-string."""

  for term in PascalCaseSplit(value):
    trie.num_strings = trie.num_strings + 1
    if term in trie.children:
      trie = trie.children[term]
    else:
      subtrie = TrieNode()
      trie.children[term] = subtrie
      trie = subtrie

  trie.num_strings = trie.num_strings + 1


def ComputeWildcardsFromTrie(trie, min_depth, min_cases):
  """Computes a list of wildcarded test case names from a trie using a depth
  first traversal."""

  WILDCARD = '*'

  # Stack of values to process, initialized with the root node.
  # The first item of the tuple is the substring represented by the traversal so
  # far.
  # The second item of the tuple is the TrieNode itself.
  # The third item is the depth of the traversal so far.
  to_process = [('', trie, 0)]

  while len(to_process) > 0:
    cur_prefix, cur_trie, cur_depth = to_process.pop()
    assert (cur_trie.num_strings != 0)

    if len(cur_trie.children) == 0:
      # No more children == we're at the end of a string.
      yield cur_prefix

    elif (cur_depth == min_depth) and \
         cur_trie.num_strings > min_cases:
      # Trim traversal of this path if the path is deep enough and there
      # are enough entries to warrant elision.
      yield cur_prefix + WILDCARD

    else:
      # Traverse all children of this node.
      for term, subtrie in cur_trie.children.items():
        to_process.append((cur_prefix + term, subtrie, cur_depth + 1))


def CompressWithWildcards(test_list, min_depth, min_cases):
  """Given a list of SUITE.CASE names, generates an exclusion list using
  wildcards to reduce redundancy.
  For example:
    Foo.TestOne
    Foo.TestTwo
  becomes:
    Foo.Test*"""

  suite_tries = {}

  # First build up a trie based representations of all test case names,
  # partitioned per-suite.
  for case in test_list:
    suite_name, test = case.split('.')
    if not suite_name in suite_tries:
      suite_tries[suite_name] = TrieNode()
    TrieInsert(suite_tries[suite_name], test)

  output = []
  # Go through the suites' tries and generate wildcarded representations
  # of the cases.
  for suite in suite_tries.items():
    suite_name, cases_trie = suite
    for case_wildcard in ComputeWildcardsFromTrie(cases_trie, min_depth, \
            min_cases):
      output.append("{}.{}".format(suite_name, case_wildcard))

  output.sort()
  return output


def GetFailedTestsFromTestLauncherSummary(summary):
  failures = set()
  for iteration in summary['per_iteration_data']:
    for case_name, results in iteration.items():
      for result in results:
        if result['status'] == 'FAILURE':
          failures.add(case_name)
  return list(failures)


def GetFiltersForTests(tests, class_only):
  # Note: Test names have the following structures:
  #  * FixtureName.TestName
  #  * InstantiationName/FixtureName.TestName/## (for TEST_P)
  #  * InstantiationName/FixtureName/ParameterId.TestName (for TYPED_TEST_P)
  #  * FixtureName.TestName/##
  #  * FixtureName/##.TestName (for TYPED_TEST)
  # Since this script doesn't parse instantiations, we generate filters to
  # match either regular tests or instantiated tests.
  if class_only:
    fixtures = set([t.split('.')[0] for t in tests])
    return [c + '.*' for c in fixtures] + \
          ['*/' + c + '.*/*' for c in fixtures] + \
          ['*/' + c + '/*.*' for c in fixtures] + \
          [c + '.*/*' for c in fixtures] + \
          [c + '/*.*' for c in fixtures]
  else:
    fixtures_and_tcs = [test.split('.', 1) for test in tests]
    return [c for c in tests] + \
        ['*/' + c + '/*' for c in tests] + \
        [c + '/*' for c in tests] + \
        [fixture + '/*.' + tc for fixture, tc in fixtures_and_tcs]


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--input-format',
      choices=['swarming_summary', 'test_launcher_summary', 'test_file'],
      default='test_file')
  parser.add_argument('--output-format',
                      choices=['file', 'args'],
                      default='args')
  parser.add_argument('--wildcard-compress', action='store_true')
  parser.add_argument(
      '--wildcard-min-depth',
      type=int,
      default=1,
      help="Minimum number of terms in a case before a wildcard may be " +
      "used, so that prefixes are not excessively broad.")
  parser.add_argument(
      '--wildcard-min-cases',
      type=int,
      default=3,
      help="Minimum number of cases in a filter before folding into a " +
      "wildcard, so as to not create wildcards needlessly for small "
      "numbers of similarly named test failures.")
  parser.add_argument('--line', type=int)
  parser.add_argument('--class-only', action='store_true')
  parser.add_argument(
      '--as-exclusions',
      action='store_true',
      help='Generate exclusion rules for test cases, instead of inclusions.')
  args, left = parser.parse_known_args()

  test_filters = []
  if args.input_format == 'swarming_summary':
    # Decode the JSON files separately and combine their contents.
    test_filters = []
    for json_file in left:
      test_filters.extend(json.loads('\n'.join(open(json_file, 'r'))))

    if args.wildcard_compress:
      test_filters = CompressWithWildcards(test_filters,
                                           args.wildcard_min_depth,
                                           args.wildcard_min_cases)

  elif args.input_format == 'test_launcher_summary':
    # Decode the JSON files separately and combine their contents.
    test_filters = []
    for json_file in left:
      test_filters.extend(
          GetFailedTestsFromTestLauncherSummary(
              json.loads('\n'.join(open(json_file, 'r')))))

    if args.wildcard_compress:
      test_filters = CompressWithWildcards(test_filters,
                                           args.wildcard_min_depth,
                                           args.wildcard_min_cases)

  else:
    file_input = fileinput.input(left)
    if args.line:
      # If --line is used, restrict text to a few lines around the requested
      # line.
      requested_line = args.line
      selected_lines = []
      for line in file_input:
        if (fileinput.lineno() >= requested_line
            and fileinput.lineno() <= requested_line + 1):
          selected_lines.append(line)
      txt = ''.join(selected_lines)
    else:
      txt = ''.join(list(file_input))

    # This regex is not exhaustive, and should be updated as needed.
    rx = re.compile(
        r'^(?:TYPED_)?(?:IN_PROC_BROWSER_)?TEST(_F|_P)?\(\s*(\w+)\s*' + \
            r',\s*(\w+)\s*\)',
        flags=re.DOTALL | re.M)
    tests = []
    for m in rx.finditer(txt):
      tests.append(m.group(2) + '.' + m.group(3))

    if args.wildcard_compress:
      test_filters = CompressWithWildcards(tests, args.wildcard_min_depth,
                                           args.wildcard_min_cases)
    else:
      test_filters = GetFiltersForTests(tests, args.class_only)

  if args.as_exclusions:
    test_filters = ['-' + x for x in test_filters]

  if args.output_format == 'file':
    print('\n'.join(test_filters))
  else:
    print(':'.join(test_filters))

  return 0


if __name__ == '__main__':
  sys.exit(main())
