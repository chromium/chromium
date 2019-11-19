#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Make sure all of the per-file *_messages.h OWNERS are consistent"""

from __future__ import print_function

import os
import re
import sys

def main():
  file_path = os.path.dirname(__file__);
  root_dir = os.path.abspath(os.path.join(file_path, '..', '..'))
  owners = collect_owners(root_dir)
  all_owners = get_all_owners(owners)
  print_missing_owners(owners, all_owners)
  return 0

def collect_owners(root_dir):
  result = {}
  for root, dirs, files in os.walk(root_dir):
    if "OWNERS" in files:
      owner_file_path = os.path.join(root, "OWNERS")
      owner_set = extract_owners_from_file(owner_file_path)
      if owner_set:
        result[owner_file_path] = owner_set
  return result

def extract_owners_from_file(owner_file_path):
  result = set()
  regexp = re.compile('^per-file.*_messages[^=]*=\s*(.*)@([^#]*)')
  with open(owner_file_path) as f:
    for line in f:
      match = regexp.match(line)
      if match:
        result.add(match.group(1).strip())
  return result

def get_all_owners(owner_dict):
  result = set()
  for key in owner_dict:
    result = result.union(owner_dict[key])
  return result

def print_missing_owners(owner_dict, owner_set):
  for key in owner_dict:
    for owner in owner_set:
      if not owner in owner_dict[key]:
        print(key + " is missing " + owner)


if '__main__' == __name__:
  sys.exit(main())
