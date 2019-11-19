#!/usr/bin/env python
# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Reads lines from files or stdin and identifies C++ tests.

Outputs a filter that can be used with --gtest_filter to run only the tests
identified.

Usage:

Outputs filter for all test fixtures in a directory. --class-only avoids an
overly long filter string.
> cat components/mycomp/**test.cc | make-gtest-filter.py --class-only

Outputs filter for all tests in a file.
> make-gtest-filter.py ./myfile_unittest.cc

Outputs filter for only test at line 123
> make-gtest-filter.py --line=123 ./myfile_unittest.cc
"""

from __future__ import print_function

import argparse
import fileinput
import re
import sys

parser = argparse.ArgumentParser()
parser.add_argument('--line', type=int)
parser.add_argument('--class-only', action='store_true')
args, left = parser.parse_known_args()
file_input = fileinput.input(left)

if args.line:
  # If --line is used, restrict text to a few lines around the requested
  # line.
  requested_line = args.line
  selected_lines = []
  for line in file_input:
    if (fileinput.lineno() >= requested_line and
        fileinput.lineno() <= requested_line + 1):
      selected_lines.append(line)
  txt = ''.join(selected_lines)
else:
  txt = ''.join(list(file_input))

# This regex is not exhaustive, and should be updated as needed.
rx = re.compile(
    r'^(?:TYPED_)?(?:IN_PROC_BROWSER_)?TEST(_F|_P)?\(\s*(\w+)\s*,\s*(\w+)\s*\)',
    flags=re.DOTALL | re.M)
tests = []
for m in rx.finditer(txt):
  tests.append(m.group(2) + '.' + m.group(3))

# Note: Test names have the following structures:
#  * FixtureName.TestName
#  * InstantiationName/FixtureName.TestName/##
# Since this script doesn't parse instantiations, we generate filters to match
# either regular tests or instantiated tests.
if args.class_only:
  fixtures = set([t.split('.')[0] for t in tests])
  test_filters = [c + '.*' for c in fixtures]
  instantiation_filters = ['*/' + c + '.*/*' for c in fixtures]
  print(':'.join(test_filters + instantiation_filters))
else:
  instantiations = ['*/' + c + '/*' for c in tests]
  print(':'.join(tests + instantiations))
