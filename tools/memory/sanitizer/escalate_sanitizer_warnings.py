#!/usr/bin/env python3

# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script searches for sanitizer warnings in stdout that do not trigger
test failures in process, and when it finds one, it updates the test status
to FAILURE for that test case.

This script is added in test_env.py after the test process runs, but before
the results are returned to result_adpater."""

import argparse
import json
import re
"""
We are looking for various types of sanitizer warnings. They have different
formats but all end in a line 'SUMMARY: (...)Sanitizer:'.
asan example:
=================================================================
==244157==ERROR: AddressSanitizer: heap-use-after-free on address 0x60b0000000f0
...
SUMMARY: AddressSanitizer:

lsan example:
=================================================================
==23646==ERROR: LeakSanitizer: detected memory leaks
...
SUMMARY: AddressSanitizer: 7 byte(s) leaked in 1 allocation(s)

msan example:
==51936==WARNING: MemorySanitizer: use-of-uninitialized-value
...
SUMMARY: MemorySanitizer: use-of-uninitialized-value (/tmp/build/a.out+0x9fd9a)

tsan example:
==================
WARNING: ThreadSanitizer: data race (pid=14909)
...
SUMMARY: ThreadSanitizer: data race base/.../typed_macros_internal.cc:135:8...

ubsan example:
../../content/browser/.../render_widget_host_impl.cc:603:35: runtime error:
member call on address 0x28dc001c8a00 which does not point to an object of type
...
SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior
"""
_SUMMARY_MESSAGE_STR = r'\nSUMMARY: (Address|Leak|Memory|Thread|UndefinedBehavior)Sanitizer:'
_SUMMARY_MESSAGE_REGEX = re.compile(_SUMMARY_MESSAGE_STR)


def escalate_test_status(test_name, test_run):
  original_status = test_run['status']
  # If test was not a SUCCESS, do not change it.
  if original_status != 'SUCCESS':
    return

  regex_result = _SUMMARY_MESSAGE_REGEX.search(test_run['output_snippet'])
  if regex_result:
    sanitizer_type = regex_result.groups()[0]
    print('Found %sSanitizer Issue in test "%s"' % (sanitizer_type, test_name))
    test_run['original_status'] = test_run['status']
    test_run['status'] = 'FAILURE'
    test_run['status_processed_by'] = 'escalate_sanitizer_warnings.py'


def escalate_sanitizer_warnings(filename):
  with open(filename, 'r') as f:
    json_data = json.load(f)

  for iteration_data in json_data['per_iteration_data']:
    for test_name, test_runs in iteration_data.items():
      for test_run in test_runs:
        escalate_test_status(test_name, test_run)

  with open(filename, 'w') as f:
    json.dump(json_data, f, indent=3, sort_keys=True)


def main():
  parser = argparse.ArgumentParser(description='Escalate sanitizer warnings.')
  parser.add_argument(
      '--test-summary-json-file',
      required=True,
      help='Path to a JSON file produced by the test launcher. The script will '
      'parse output snippets to find sanitizer warnings that are shown as '
      'WARNINGS but should cause build failures in sanitizer versions. The '
      'status will be FAILED when found. The result will be written back '
      'to the JSON file.')
  args = parser.parse_args()

  if args.test_summary_json_file:
    escalate_sanitizer_warnings(args.test_summary_json_file)
  else:
    print("WARNING: summary json file is required")


if __name__ == '__main__':
  main()
