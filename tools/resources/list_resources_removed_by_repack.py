#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import os
import re
import sys

usage = """%s BUILDTYPE BUILDDIR

BUILDTYPE: either chromium or chrome.
BUILDDIR: The path to the output directory. e.g. relpath/to/out/Release

Prints out (to stdout) the sorted list of resource ids that are marked as
unused during the repacking process in the given build log (via stdin).
Additionally, attempt to print out the name of the resource and the generated
header file that contains the resource.

This script is used to print the list of resources that are not used so that
developers will notice and fix their .grd files.
"""


def GetResourceIdsFromRepackMessage(in_data):
  """Returns sorted set of resource ids that are not used from in_data.
  """
  unused_resources = set()
  unused_pattern = re.compile(
      'RePackFromDataPackStrings Removed Key: (?P<resource_id>[0-9]+)')
  for line in in_data:
    match = unused_pattern.match(line)
    if match:
      resource_id = int(match.group('resource_id'))
      unused_resources.add(resource_id)
  return sorted(unused_resources)


def Main():
  if len(sys.argv) != 3:
    sys.stderr.write(usage % sys.argv[0])
    return 1

  build_type = sys.argv[1]
  build_dir = sys.argv[2]

  if build_type not in ('chromium', 'chrome'):
    sys.stderr.write(usage % sys.argv[0])
    return 1

  generated_output_dir = os.path.join(build_dir, 'gen')
  if not os.path.exists(generated_output_dir):
    sys.stderr.write('Cannot find gen dir %s' % generated_output_dir)
    return 1

  product = 'chromium' if build_type == 'chromium' else 'google_chrome'
  suffix = product + '_strings.h'
  excluded_headers = set([s % suffix for s in ('%s', 'components_%s')])

  data_files = []
  for root, dirs, files in os.walk(generated_output_dir):
    if os.path.basename(root) != 'grit':
      continue

    header_files = set([header for header in files if header.endswith('.h')])
    header_files -= excluded_headers
    data_files.extend([os.path.join(root, header) for header in header_files])

  resource_id_to_name_file_map = {}
  resource_pattern = re.compile('#define (?P<resource_name>[A-Z0-9_]+).* '
                                '(?P<resource_id>[0-9]+)$')
  for f in data_files:
    data = open(f).read()
    for line in data.splitlines():
      match = resource_pattern.match(line)
      if match:
        resource_id = int(match.group('resource_id'))
        resource_name = match.group('resource_name')
        if resource_id in resource_id_to_name_file_map:
          print('Duplicate:', resource_id)
          print(resource_name, f)
          print(resource_id_to_name_file_map[resource_id])
          raise
        resource_id_to_name_file_map[resource_id] = (resource_name, f)

  unused_resources = GetResourceIdsFromRepackMessage(sys.stdin)
  for resource_id in unused_resources:
    if resource_id not in resource_id_to_name_file_map:
      print('WARNING: Unknown resource id', resource_id)
      continue
    (resource_name, filename) = resource_id_to_name_file_map[resource_id]
    sys.stdout.write('%d: %s in %s\n' % (resource_id, resource_name, filename))
  return 0


if __name__ == '__main__':
  sys.exit(Main())
