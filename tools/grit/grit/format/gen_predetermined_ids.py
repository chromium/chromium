#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
A tool to generate a predetermined resource ids file that can be used as an
input to grit via the -p option. This is meant to be run manually every once in
a while and its output checked in. See tools/gritsettings/README.md for details.
"""

from __future__ import print_function

import os
import re
import sys

# Regular expression for parsing the #define macro format. Matches both the
# version of the macro with whitelist support and the one without. For example,
# Without generate whitelist flag:
#   #define IDS_FOO_MESSAGE 1234
# With generate whitelist flag:
#   #define IDS_FOO_MESSAGE (::ui::WhitelistedResource<1234>(), 1234)
RESOURCE_EXTRACT_REGEX = re.compile(r'^#define (\S*).* (\d+)\)?$', re.MULTILINE)

ORDERED_RESOURCE_IDS_REGEX = re.compile(r'^Resource=(\d*)$', re.MULTILINE)


def _GetResourceNameIdPairsIter(string_to_scan):
  """Gets an iterator of the resource name and id pairs of the given string.

  Scans the input string for lines of the form "#define NAME ID" and returns
  an iterator over all matching (NAME, ID) pairs.

  Args:
    string_to_scan: The input string to scan.

  Yields:
    A tuple of name and id.
  """
  for match in RESOURCE_EXTRACT_REGEX.finditer(string_to_scan):
    yield match.group(1, 2)


def _ReadOrderedResourceIds(path):
  """Reads ordered resource ids from the given file.

  The resources are expected to be of the format produced by running Chrome
  with --print-resource-ids command line.

  Args:
    path: File path to read resource ids from.

  Returns:
    An array of ordered resource ids.
  """
  ordered_resource_ids = []
  with open(path, "r") as f:
    for match in ORDERED_RESOURCE_IDS_REGEX.finditer(f.read()):
      ordered_resource_ids.append(int(match.group(1)))
  return ordered_resource_ids


def GenerateResourceMapping(original_resources, ordered_resource_ids):
  """Generates a resource mapping from the ordered ids and the original mapping.

  The returned dict will assign new ids to ordered_resource_ids numerically
  increasing from 101.

  Args:
    original_resources: A dict of original resource ids to resource names.
    ordered_resource_ids: An array of ordered resource ids.

  Returns:
    A dict of resource ids to resource names.
  """
  output_resource_map = {}
  # 101 is used as the starting value since other parts of GRIT require it to be
  # the minimum (e.g. rc_header.py) based on Windows resource numbering.
  next_id = 101
  for original_id in ordered_resource_ids:
    resource_name = original_resources[original_id]
    output_resource_map[next_id] = resource_name
    next_id += 1
  return output_resource_map


def ReadResourceIdsFromFile(file, original_resources):
  """Reads resource ids from a GRIT-produced header file.

  Args:
    file: File to a GRIT-produced header file to read from.
    original_resources: Dict of resource ids to resource names to add to.
  """
  for resource_name, resource_id in _GetResourceNameIdPairsIter(file.read()):
    original_resources[int(resource_id)] = resource_name


def _ReadOriginalResourceIds(out_dir):
  """Reads resource ids from GRIT header files in the specified directory.

  Args:
    out_dir: A Chrome build output directory (e.g. out/gn) to scan.

  Returns:
    A dict of resource ids to resource names.
  """
  original_resources = {}
  for root, dirnames, filenames in os.walk(out_dir + '/gen'):
    for filename in filenames:
      if filename.endswith(('_resources.h', '_settings.h', '_strings.h')):
        with open(os.path.join(root, filename), "r") as f:
          ReadResourceIdsFromFile(f, original_resources)
  return original_resources


def _GeneratePredeterminedIdsFile(ordered_resources_file, out_dir):
  """Generates a predetermined ids file.

  Args:
    ordered_resources_file: File path to read ordered resource ids from.
    out_dir: A Chrome build output directory (e.g. out/gn) to scan.

  Returns:
    A dict of resource ids to resource names.
  """
  original_resources = _ReadOriginalResourceIds(out_dir)
  ordered_resource_ids = _ReadOrderedResourceIds(ordered_resources_file)
  output_resource_map = GenerateResourceMapping(original_resources,
                                                ordered_resource_ids)
  for res_id in sorted(output_resource_map.keys()):
    print(output_resource_map[res_id], res_id)


def main(argv):
  if len(argv) != 2:
    print("usage: gen_predetermined_ids.py <ordered_resources_file> <out_dir>")
    sys.exit(1)
  ordered_resources_file, out_dir = argv[0], argv[1]
  _GeneratePredeterminedIdsFile(ordered_resources_file, out_dir)


if '__main__' == __name__:
  main(sys.argv[1:])
