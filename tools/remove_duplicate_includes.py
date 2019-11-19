#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script will search through the target folder specified and try to find
duplicate includes from h and cc files, and remove them from the cc files. The
current/working directory needs to be chromium_checkout/src/ when this tool is
run.

Usage: remove_duplicate_includes.py --dry-run components/foo components/bar
"""

from __future__ import print_function

import argparse
import collections
import logging
import os
import re
import sys

# This could be generalized if desired, and moved to command line arguments.
H_FILE_SUFFIX = '.h'
CC_FILE_SUFFIX = '.cc'

# The \s should allow us to ignore any whitespace and only focus on the group
# captured when comparing between files.
INCLUDE_REGEX = re.compile('^\s*(#include\s+[\"<](.*?)[\">])\s*$')

def HasSuffix(file_name, suffix):
  return os.path.splitext(file_name)[1] == suffix

def IsEmpty(line):
  return not line.strip()

def FindIncludeSet(input_lines, h_path_to_include_set, cc_file_name):
  """Finds and returns the corresponding include set for the given .cc file.

  This is done by finding the first include in the file and then trying to look
  up an .h file in the passed in map. If not present, then None is returned
  immediately.
  """
  for line in input_lines:
    match = INCLUDE_REGEX.search(line)
    # The first include match should be the corresponding .h file, else skip.
    if match:
      h_file_path = os.path.join(os.getcwd(), match.group(2))
      if h_file_path not in h_path_to_include_set:
        print('First include did not match to a known .h file, skipping ' + \
          cc_file_name + ', line: ' + match.group(1))
        return None
      return h_path_to_include_set[h_file_path]

def WithoutDuplicates(input_lines, include_set, cc_file_name):
  """Checks every input line and sees if we can remove it based on the contents
  of the given include set.

  Returns what the new contents of the file should be.
  """
  output_lines = []
  # When a section of includes are completely removed, we want to remove the
  # trailing empty as well.
  lastCopiedLineWasEmpty = False
  lastLineWasOmitted = False
  for line in input_lines:
      match = INCLUDE_REGEX.search(line)
      if match and match.group(2) in include_set:
        print('Removed ' + match.group(1) + ' from ' + cc_file_name)
        lastLineWasOmitted = True
      elif lastCopiedLineWasEmpty and lastLineWasOmitted and IsEmpty(line):
        print('Removed empty line from ' + cc_file_name)
        lastLineWasOmitted = True
      else:
        lastCopiedLineWasEmpty = IsEmpty(line)
        lastLineWasOmitted = False
        output_lines.append(line)
  return output_lines

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--dry-run', action='store_true',
    help='Does not actually remove lines when specified.')
  parser.add_argument('targets', nargs='+',
    help='Relative path to folders to search for duplicate includes in.')
  args = parser.parse_args()

  # A map of header file paths to the includes they contain.
  h_path_to_include_set = {}

  # Simply collects the path of all cc files present.
  cc_file_path_set = set()

  for relative_root in args.targets:
    absolute_root = os.path.join(os.getcwd(), relative_root)
    for dir_path, dir_name_list, file_name_list in os.walk(absolute_root):
      for file_name in file_name_list:
        file_path = os.path.join(dir_path, file_name)
        if HasSuffix(file_name, H_FILE_SUFFIX):
          # By manually adding the set instead of using defaultdict we can avoid
          # warning about missing .h files when the .h file has no includes.
          h_path_to_include_set[file_path] = set()
          with open(file_path) as fh:
            for line in fh:
              match = INCLUDE_REGEX.search(line)
              if match:
                h_path_to_include_set[file_path].add(match.group(2))
        elif HasSuffix(file_name, CC_FILE_SUFFIX):
          cc_file_path_set.add(file_path)

  for cc_file_path in cc_file_path_set:
    cc_file_name = os.path.basename(cc_file_path)
    with open(cc_file_path, 'r' if args.dry_run else 'r+') as fh:
      # Read out all lines and reset file position to allow overwriting.
      input_lines = fh.readlines()
      fh.seek(0)
      include_set = FindIncludeSet(input_lines, h_path_to_include_set,
                                   cc_file_name)
      if include_set:
        output_lines = WithoutDuplicates(input_lines, include_set, cc_file_name)
        if not args.dry_run:
          fh.writelines(output_lines)
          fh.truncate()

if __name__ == '__main__':
  sys.exit(main())
